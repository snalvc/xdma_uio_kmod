
#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/version.h>

#include "xdma_uio.h"

struct xdma_uio_pci_dev {
  struct uio_info info;
  struct pci_dev *pdev;
  // enum rte_intr_mode mode;
};

static void pci_check_intr_pend(struct pci_dev *pdev) {
  u16 v;

  pci_read_config_word(pdev, PCI_STATUS, &v);
  if (v & PCI_STATUS_INTERRUPT) {
    pr_info("%s PCI STATUS Interrupt pending 0x%x.\n", dev_name(&pdev->dev), v);
    pci_write_config_word(pdev, PCI_STATUS, PCI_STATUS_INTERRUPT);
  }
}

/* Remap pci resources described by bar #pci_bar in uio resource n. */
static int uio_setup_iomem(struct pci_dev *pdev, struct uio_info *info, int n,
                           int pci_bar, const char *name) {
  unsigned long addr, len;
  void *internal_addr;

  if (n >= ARRAY_SIZE(info->mem))
    return -EINVAL;

  addr = pci_resource_start(pdev, pci_bar);
  len = pci_resource_len(pdev, pci_bar);
  if (addr == 0 || len == 0)
    return -1;
  internal_addr = ioremap(addr, len);
  if (internal_addr == NULL)
    return -1;
  info->mem[n].name = name;
  info->mem[n].addr = addr;
  info->mem[n].internal_addr = internal_addr;
  info->mem[n].size = len;
  info->mem[n].memtype = UIO_MEM_PHYS;
  return 0;
}

/* Get pci port io resources described by bar #pci_bar in uio resource n. */
static int uio_setup_ioport(struct pci_dev *pdev, struct uio_info *info, int n,
                            int pci_bar, const char *name) {
  unsigned long addr, len;

  if (n >= ARRAY_SIZE(info->port))
    return -EINVAL;

  addr = pci_resource_start(pdev, pci_bar);
  len = pci_resource_len(pdev, pci_bar);
  if (addr == 0 || len == 0)
    return -EINVAL;

  info->port[n].name = name;
  info->port[n].start = addr;
  info->port[n].size = len;
  info->port[n].porttype = UIO_PORT_X86;

  return 0;
}

/* Unmap previously ioremap'd resources */
static void igbuio_pci_release_iomem(struct uio_info *info) {
  int i;

  for (i = 0; i < MAX_UIO_MAPS; i++) {
    if (info->mem[i].internal_addr)
      iounmap(info->mem[i].internal_addr);
  }
}

static int xdma_uio_setup_bars(struct pci_dev *pdev,
                               struct xdma_uio_pci_dev *xuio) {
  int i, iom, iop, ret;
  unsigned long flags;
  static const char *bar_names[PCI_STD_RESOURCE_END + 1] = {
      "BAR0", "BAR1", "BAR2", "BAR3", "BAR4", "BAR5",
  };

  iom = 0;
  iop = 0;

  for (i = 0; i < ARRAY_SIZE(bar_names); i++) {
    if (pci_resource_len(pdev, i) != 0 && pci_resource_start(pdev, i) != 0) {
      flags = pci_resource_flags(pdev, i);
      if (flags & IORESOURCE_MEM) {
        ret = uio_setup_iomem(pdev, &xuio->info, iom, i, bar_names[i]);
        if (ret != 0)
          return ret;
        iom++;
      } else if (flags & IORESOURCE_IO) {
        ret = uio_setup_ioport(pdev, &xuio->info, iop, i, bar_names[i]);
        if (ret != 0)
          return ret;
        iop++;
      }
    }
  }

  return (iom != 0 || iop != 0) ? ret : -ENOENT;
}

#if KERNEL_VERSION(3, 5, 0) <= LINUX_VERSION_CODE
static void pci_enable_capability(struct pci_dev *pdev, int cap) {
  pcie_capability_set_word(pdev, PCI_EXP_DEVCTL, cap);
}
#else
static void pci_enable_capability(struct pci_dev *pdev, int cap) {
  u16 v;
  int pos;

  pos = pci_pcie_cap(pdev);
  if (pos > 0) {
    pci_read_config_word(pdev, pos + PCI_EXP_DEVCTL, &v);
    v |= cap;
    pci_write_config_word(pdev, pos + PCI_EXP_DEVCTL, v);
  }
}
#endif

static int xdma_uio_pci_probe(struct pci_dev *pdev,
                              const struct pci_device_id *id) {
  struct xdma_uio_pci_dev *udev;
  int rv;

  udev = kzalloc(sizeof(struct xdma_uio_pci_dev), GFP_KERNEL);
  if (!udev)
    return -ENOMEM;

  rv = pci_enable_device(pdev);
  if (rv != 0) {
    pr_err("Failed to enable PCI device\n");
    goto out_free_udev;
  }

  /* enable bus mastering on the device */
  pci_set_master(pdev);

  pci_check_intr_pend(pdev);

  /* enable relaxed ordering */
  pci_enable_capability(pdev, PCI_EXP_DEVCTL_RELAX_EN);

  /* enable extended tag */
  pci_enable_capability(pdev, PCI_EXP_DEVCTL_EXT_TAG);

  /* force MRRS to be 512 */
  rv = pcie_set_readrq(pdev, 512);
  if (rv)
    pr_info("Failed to set PCI_EXP_DEVCTL_READRQ: %d.\n", rv);

  /* remap IO memory */
  rv = xdma_uio_setup_bars(pdev, udev);
  if (rv != 0)
    goto fail_release_iomem;

  return 0;

out_free_udev:
  kfree(udev);
fail_release_iomem:
  igbuio_pci_release_iomem(&udev->info);

  return rv;
}

static void xdma_uio_pci_remove(struct pci_dev *dev) {}

static struct pci_driver xdma_uio_pci_driver = {
    .name = "xdma_uio",
    .id_table = NULL,
    .probe = xdma_uio_pci_probe,
    .remove = xdma_uio_pci_remove,
};

static int __init xdma_uio_pci_init_module(void) {

  return pci_register_driver(&xdma_uio_pci_driver);
}

static void __exit xdma_uio_pci_exit_module(void) {
  pci_unregister_driver(&xdma_uio_pci_driver);
}

module_init(xdma_uio_pci_init_module);
module_exit(xdma_uio_pci_exit_module);

MODULE_DESCRIPTION("UIO driver for Intel IGB PCI cards");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Intel Corporation");
