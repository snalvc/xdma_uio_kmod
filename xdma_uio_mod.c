
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

static int xdma_uio_pci_probe(struct pci_dev *dev,
                              const struct pci_device_id *id) {
  return 0;
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
