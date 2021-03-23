
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

  return 0;

out_free_udev:
  kfree(udev);

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
