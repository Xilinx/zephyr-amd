.. zephyr:code-sample:: dma
   :name: DMA test
   :relevant-api: dma_interface

   Direct mode of transfer.

Overview
********

This sample demonstrates how to use the dma driver with a simple direct
mode of transfer. It transfer data directly by writing the source and
destination address in the corresponding registers and also writing the
length of bytes to be transferred.
By default, the DMA that is normally used for the Zephyr shell
is used, so that almost every board should be supported.

Scatter-gather mode can be enabled by building with
``CONFIG_DMA_TEST_SG_MODE=y``.

Write-only mode can be enabled by building with
``CONFIG_DMA_TEST_WRITE_ONLY_MODE=y``. Each channel sets
``channel_direction`` to ``XILINX_ADMA_DIR_WRITE_ONLY`` (see
:zephyr_file:`include/zephyr/drivers/dma/dma_xilinx_adma.h`) instead of
``MEMORY_TO_MEMORY``, and each channel repeatedly writes a fixed 2-word
pattern to the destination address/size instead of copying from a source
buffer. It cannot be combined with ``CONFIG_DMA_TEST_SG_MODE``.

Building and Running
********************

Build and flash the sample as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/dma/dmatest
   :board: versalnet_rpu
   :goals: build flash
