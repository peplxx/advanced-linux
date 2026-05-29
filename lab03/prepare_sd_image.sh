#!/bin/bash
#
# prepare_sd_image.sh
# Creates a complete SD card image from PetaLinux build artifacts
# Usage: ./prepare_sd_image.sh [petalinux_images_dir] [output_image]
#

set -euo pipefail

# ─── Configuration ───────────────────────────────────────────────────────────
IMAGES_DIR="${1:-images/linux}"
OUTPUT_IMAGE="${2:-sd_card.img}"
IMAGE_SIZE_MB=8192
BOOT_SIZE_MB=512
BOOT_LABEL="BOOT"
ROOTFS_LABEL="rootfs"

# ─── Color helpers ───────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# ─── Prerequisite checks ────────────────────────────────────────────────────
check_deps() {
    info "Checking dependencies..."
    local missing=()
    for cmd in dd parted mkfs.vfat mkfs.ext4 losetup mount umount tar sync; do
        if ! command -v "$cmd" &>/dev/null; then
            missing+=("$cmd")
        fi
    done
    if [ ${#missing[@]} -ne 0 ]; then
        error "Missing tools: ${missing[*]}\n  Install with: sudo apt install -y parted dosfstools e2fsprogs"
    fi
    ok "All dependencies found"
}

check_artifacts() {
    info "Checking build artifacts in: ${IMAGES_DIR}/"
    local required=(BOOT.BIN image.ub boot.scr rootfs.tar.gz)
    local all_found=true

    for f in "${required[@]}"; do
        if [ -f "${IMAGES_DIR}/${f}" ]; then
            local size
            size=$(du -h "${IMAGES_DIR}/${f}" | cut -f1)
            ok "  ${f} (${size})"
        else
            warn "  ${f} — NOT FOUND"
            all_found=false
        fi
    done

    if [ "$all_found" = false ]; then
        error "Missing required artifacts. Run petalinux-build and petalinux-package first."
    fi
    ok "All artifacts present"
}

check_root() {
    if [ "$(id -u)" -ne 0 ]; then
        error "This script must be run as root (sudo).\n  Usage: sudo $0 $*"
    fi
}

# ─── Cleanup handler ────────────────────────────────────────────────────────
LOOP_DEV=""
BOOT_MOUNTED=false
ROOTFS_MOUNTED=false

cleanup() {
    info "Cleaning up..."
    if [ "$BOOT_MOUNTED" = true ]; then
        umount /mnt/zynq_boot 2>/dev/null || true
    fi
    if [ "$ROOTFS_MOUNTED" = true ]; then
        umount /mnt/zynq_rootfs 2>/dev/null || true
    fi
    if [ -n "$LOOP_DEV" ]; then
        losetup -d "$LOOP_DEV" 2>/dev/null || true
    fi
    rmdir /mnt/zynq_boot 2>/dev/null || true
    rmdir /mnt/zynq_rootfs 2>/dev/null || true
}
trap cleanup EXIT

# ─── Step 1: Create empty image file ────────────────────────────────────────
create_image() {
    info "Creating ${IMAGE_SIZE_MB} MB image file: ${OUTPUT_IMAGE}"
    dd if=/dev/zero of="${OUTPUT_IMAGE}" bs=1M count="${IMAGE_SIZE_MB}" status=progress
    ok "Image file created ($(du -h "${OUTPUT_IMAGE}" | cut -f1))"
}

# ─── Step 2: Partition the image ────────────────────────────────────────────
partition_image() {
    info "Partitioning image..."
    parted -s "${OUTPUT_IMAGE}" \
        mklabel msdos \
        mkpart primary fat32 1MiB "${BOOT_SIZE_MB}MiB" \
        mkpart primary ext4 "${BOOT_SIZE_MB}MiB" 100% \
        set 1 boot on

    ok "Partition table created:"
    parted -s "${OUTPUT_IMAGE}" print
}

# ─── Step 3: Setup loop device ──────────────────────────────────────────────
setup_loop() {
    info "Setting up loop device..."
    LOOP_DEV=$(losetup --find --show --partscan "${OUTPUT_IMAGE}")
    ok "Loop device: ${LOOP_DEV}"

    # Wait for partition devices to appear
    sleep 2
    partprobe "${LOOP_DEV}" 2>/dev/null || true
    sleep 1

    # Verify partitions exist
    if [ ! -b "${LOOP_DEV}p1" ] || [ ! -b "${LOOP_DEV}p2" ]; then
        # Some systems use different naming
        if [ -b "${LOOP_DEV}p1" ]; then
            :
        elif [ -b "$(echo "${LOOP_DEV}" | sed 's/loop/loop/')p1" ]; then
            :
        else
            error "Partition devices not found. Expected ${LOOP_DEV}p1 and ${LOOP_DEV}p2"
        fi
    fi

    ok "Partitions: ${LOOP_DEV}p1 (boot), ${LOOP_DEV}p2 (rootfs)"
}

# ─── Step 4: Format partitions ──────────────────────────────────────────────
format_partitions() {
    info "Formatting partition 1 as FAT32 (${BOOT_LABEL})..."
    mkfs.vfat -F 32 -n "${BOOT_LABEL}" "${LOOP_DEV}p1"
    ok "FAT32 partition formatted"

    info "Formatting partition 2 as EXT4 (${ROOTFS_LABEL})..."
    mkfs.ext4 -L "${ROOTFS_LABEL}" -F "${LOOP_DEV}p2"
    ok "EXT4 partition formatted"
}

# ─── Step 5: Mount and populate BOOT partition ──────────────────────────────
populate_boot() {
    info "Mounting and populating BOOT partition..."
    mkdir -p /mnt/zynq_boot
    mount "${LOOP_DEV}p1" /mnt/zynq_boot
    BOOT_MOUNTED=true

    cp -v "${IMAGES_DIR}/BOOT.BIN"  /mnt/zynq_boot/
    cp -v "${IMAGES_DIR}/image.ub"  /mnt/zynq_boot/
    cp -v "${IMAGES_DIR}/boot.scr"  /mnt/zynq_boot/

    # Copy uEnv.txt if it exists
    if [ -f "${IMAGES_DIR}/uEnv.txt" ]; then
        cp -v "${IMAGES_DIR}/uEnv.txt" /mnt/zynq_boot/
    fi

    echo ""
    info "BOOT partition contents:"
    ls -lh /mnt/zynq_boot/
    echo ""

    sync
    umount /mnt/zynq_boot
    BOOT_MOUNTED=false
    ok "BOOT partition populated"
}

# ─── Step 6: Mount and populate rootfs partition ────────────────────────────
populate_rootfs() {
    info "Mounting and populating rootfs partition..."
    mkdir -p /mnt/zynq_rootfs
    mount "${LOOP_DEV}p2" /mnt/zynq_rootfs
    ROOTFS_MOUNTED=true

    info "Extracting rootfs.tar.gz (this may take a few minutes)..."
    tar xzf "${IMAGES_DIR}/rootfs.tar.gz" -C /mnt/zynq_rootfs/

    echo ""
    info "rootfs top-level contents:"
    ls -lh /mnt/zynq_rootfs/
    echo ""

    sync
    umount /mnt/zynq_rootfs
    ROOTFS_MOUNTED=false
    ok "rootfs partition populated"
}

# ─── Step 7: Detach loop device ─────────────────────────────────────────────
detach_loop() {
    info "Detaching loop device..."
    losetup -d "${LOOP_DEV}"
    LOOP_DEV=""
    ok "Loop device detached"
}

# ─── Step 8: Print summary ──────────────────────────────────────────────────
print_summary() {
    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  SD CARD IMAGE READY: ${OUTPUT_IMAGE}${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "  Image size: $(du -h "${OUTPUT_IMAGE}" | cut -f1)"
    echo ""
    echo "  Partition layout:"
    echo "    1. FAT32  (${BOOT_LABEL})   — ${BOOT_SIZE_MB} MB — BOOT.BIN, image.ub, boot.scr"
    echo "    2. EXT4   (${ROOTFS_LABEL}) — remaining    — Linux root filesystem"
    echo ""
    echo -e "  ${CYAN}To flash to SD card on Linux:${NC}"
    echo "    sudo dd if=${OUTPUT_IMAGE} of=/dev/sdX bs=4M status=progress conv=fsync"
    echo ""
    echo -e "  ${CYAN}To flash to SD card on macOS:${NC}"
    echo "    diskutil list                          # find your SD card (e.g., /dev/disk4)"
    echo "    diskutil unmountDisk /dev/diskN"
    echo "    sudo dd if=${OUTPUT_IMAGE} of=/dev/rdiskN bs=4m status=progress"
    echo "    diskutil eject /dev/diskN"
    echo ""
    echo -e "  ${YELLOW}⚠  Double-check the target device! dd will overwrite without confirmation.${NC}"
    echo ""
}

# ─── Main ────────────────────────────────────────────────────────────────────
main() {
    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║   Zynq SD Card Image Builder                               ║${NC}"
    echo -e "${CYAN}║   PetaLinux artifacts → bootable .img file                  ║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""

    check_root
    check_deps
    check_artifacts

    echo ""
    info "Configuration:"
    info "  Artifacts dir : ${IMAGES_DIR}"
    info "  Output image  : ${OUTPUT_IMAGE}"
    info "  Image size    : ${IMAGE_SIZE_MB} MB"
    info "  Boot partition: ${BOOT_SIZE_MB} MB (FAT32)"
    info "  Rootfs        : remaining (EXT4)"
    echo ""
    info "Starting in 3 seconds... (Ctrl+C to cancel)"
    sleep 3

    create_image
    partition_image
    setup_loop
    format_partitions
    populate_boot
    populate_rootfs
    detach_loop
    print_summary
}

main "$@"
