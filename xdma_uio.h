#ifndef _XDMA_UIO_H_
#define _XDMA_UIO_H_

#define XDMA_C2H_CHANNEL_MAX 4
#define XDMA_H2C_CHANNEL_MAX 4
#define MAX_USER_IRQ 16

/* interrupt mode */
enum xdma_intr_mode {
  XDMA_INTR_MODE_NONE = 0,
  XDMA_INTR_MODE_LEGACY,
  XDMA_INTR_MODE_MSI,
  XDMA_INTR_MODE_MSIX
};

#endif