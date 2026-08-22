.. zephyr:code-sample:: amd_trngpsx
   :name: AMD/Xilinx TRNGPSX HRNG entropy
   :relevant-api: entropy_interface

   Read random data from the AMD/Xilinx TRNGPSX (HRNG mode) entropy driver.

Overview
********

This sample demonstrates the AMD/Xilinx TRNGPSX True Random Number Generator
entropy driver operating in HRNG (Hybrid Random Number Generator) mode. It
obtains the entropy device from the ``zephyr,entropy`` chosen node, fetches
several 256-bit random blocks using :c:func:`entropy_get_entropy` and prints
them to the console.

Requirements
************

The board must describe a TRNGPSX instance (compatible ``xlnx,pmcl-trng-11.0``)
and select it as the ``zephyr,entropy`` chosen device, for example::

   / {
       chosen {
           zephyr,entropy = &pmcbridge_0_pmcl_trng_0;
       };
   };

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/entropy/amd_trngpsx
   :board: mbv32
   :goals: build flash
   :compact:

Sample Output
*************

.. code-block:: console

   entropy device ready: <name>
   random bytes: 55 ff 68 63 fc d8 aa 0a e7 e1 59 e8 5f 1c 9d b5 ...
   random bytes: ...
   done

The random bytes differ on every run.
