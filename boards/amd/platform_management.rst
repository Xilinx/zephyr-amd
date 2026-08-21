:orphan:

.. _amd_platform_management:

AMD Versal Gen 2 Platform Management
####################################

.. note::

   This page is **guidance / documentation only** for the current release. It
   describes the platform management *use case models* that apply when running
   Zephyr on AMD Versal Gen 2 devices. It is intended to help users choose the
   right model for their system and to understand the responsibilities that come
   with each model. The corresponding driver and tooling implementation is
   delivered in a later release (see :ref:`amd_pm_roadmap`).

Overview
********

*Platform management* on Versal Gen 2 devices covers the control of shared
platform resources, primarily:

* **Clock control** -- enable/disable, gating, divider and parent selection.
* **Reset control** -- assert, de-assert and pulse of peripheral resets.
* **Power / device management** -- requesting and releasing devices and power
  domains.

Versal Gen 2 devices are heterogeneous. The same platform resources can be
visible to more than one processing cluster:

* The **APU** (Application Processing Unit) -- Arm Cortex-A cores, typically
  running a rich OS (Linux) or Zephyr, usually in a non-secure context on top of
  a firmware/monitor layer.
* The **RPU** (Real-time Processing Unit) -- Arm Cortex-R52 cores, running a
  real-time OS such as Zephyr with hard real-time requirements.
* The **PLM** (Platform Loader and Manager) running on the platform management
  controller, which owns the system-level power management firmware (the EEMI
  interface).

Because these clusters share clocks, resets and power domains, the key question
for any deployment is: *which software component is allowed to change a given
resource, and how does it do so?* The models below answer that question for the
different Zephyr deployments.

Supported boards
================

The guidance on this page applies to the Versal Gen 2 Zephyr board targets:

* ``versal2_apu`` -- Zephyr on the APU (Arm Cortex-A78).
* ``versal2_rpu`` -- Zephyr on the RPU (Arm Cortex-R52).

.. _amd_pm_use_case_models:

Use case models
***************

There are two models: RPU deployments use direct register access, and APU
deployments use SCMI. Some RPU applications also need occasional communication
with the PLM; that path is currently in planning (see
:ref:`amd_pm_rpu_plm`).

.. _amd_pm_rpu_direct:

Model 1 -- RPU Zephyr with direct register access
=================================================

This is the model for real-time workloads on the RPU (Cortex-R52).

Zephyr's clock control and reset control drivers program the platform resources
through **direct register reads and writes**, without routing requests through
the PLM.

**Why direct register access on the RPU?**

Zephyr on the RPU is used for hard real-time applications that require
deterministic behaviour with predictable, bounded response times. Routing
clock/reset/power requests through the PLM (via EEMI, or via an SCMI server that
forwards to firmware) breaks this determinism:

* The request path through the PLM does **not** provide a bounded response
  latency. The PLM may be busy servicing another task (for example, the APU
  loading a programmable-logic image), during which it cannot respond to the
  RPU. This makes the operation's latency effectively unbounded.
* Real-time customers therefore prefer direct register access, which has
  deterministic timing.

This is also consistent with the existing upstream Zephyr behaviour, which
already uses direct register writes for these operations.

**User responsibilities**

Direct register access does not, by itself, provide any hardware enforced
isolation between clusters. It is the responsibility of the system integrator to
ensure that:

* The register regions used for the platform resources managed by Zephyr are
  assigned to the RPU.
* There are no access conflicts with other processors or software components
  (Linux/other OS on the APU, other RPU cores, or the PLM). For example, if a
  peripheral such as a GEM is assigned to the APU, the RPU must not change a
  clock that would affect that peripheral.

See :ref:`amd_pm_isolation` for the recommended way to enforce these
assignments.

.. _amd_pm_rpu_plm:

RPU to PLM communication
========================

Some RPU Zephyr applications still need to talk to the PLM occasionally --
for example, to wake up the APU from the RPU, or to perform a system-level
action that only the platform management firmware can carry out.

For these cases RPU to PLM communication is needed. This is an **optional**
addition on top of :ref:`amd_pm_rpu_direct`; it does not replace direct register
access for the real-time clock/reset control path, and it does not make the RPU
platform management path bounded-latency.

Legacy EEMI as the primary platform management path for RPU Zephyr is **not**
planned. RPU to PLM communication for the occasional firmware interactions
described above is currently in planning.

.. _amd_pm_apu_scmi:

Model 2 -- APU Zephyr with SCMI
===============================

This is the model for Zephyr running on the APU (Cortex-A) in a non-secure
context.

On the APU, platform management uses the standardized **SCMI** (System Control
and Management Interface). SCMI inherently assumes a firmware/monitor layer
(for example TF-A) is present, and is the appropriate choice for APU use cases.

The main advantage of SCMI is **resource ownership and isolation**: each SCMI
client (agent) is only permitted to manage the resources it owns. This keeps
deployments where peripherals are distributed between the APU and RPU safe --
one cluster cannot reconfigure a clock or reset that belongs to another.

SCMI is **not** suitable for the RPU real-time use case, for the bounded-latency
reasons described in :ref:`amd_pm_rpu_direct`. SCMI targets application-class
processors with a firmware layer, not real-time microcontroller-class cores.

.. _amd_pm_isolation:

Resource isolation and device tree generation
**********************************************

Because the direct-access model relies on the integrator to partition
resources correctly, the recommended flow is to let the tooling generate the
device trees and enforce the isolation:

#. Use **ACE** (``isoutil``) to describe the isolation, i.e. which peripherals
   and memory regions are assigned to each cluster.
#. Export that isolation, together with the **System Device Tree**, to
   **Lopper**.
#. Lopper generates the per-cluster device trees (a Zephyr device tree and a
   Linux device tree), each containing only the peripherals and memory regions
   assigned to that cluster.

With this flow the generated Zephyr device tree only references resources that
belong to the RPU, so the direct-access model cannot accidentally touch a
resource owned by Linux or another cluster.

.. warning::

   If you author the Zephyr device tree **manually** and bypass ``isoutil`` /
   Lopper, the tooling can no longer guarantee isolation. In that case you are
   responsible for ensuring that the peripherals, clocks, resets and memory
   regions described in the Zephyr device tree are actually assigned to the RPU
   and do not conflict with the APU (Linux/other OS) or the PLM.

.. _amd_pm_roadmap:

Release roadmap
***************

The following table summarizes the direction across releases. Only the
documentation (this page and the use case models it describes) is part of the
current release; the driver and tooling work is delivered later.

.. list-table::
   :header-rows: 1
   :widths: 20 40 40

   * - Release
     - APU Zephyr
     - RPU Zephyr
   * - Current (documentation only)
     - Use case models documented as guidance (this page).
     - Use case models documented as guidance (this page).
   * - Next
     - SCMI support: direct register accesses replaced by the appropriate SCMI
       calls (see :ref:`amd_pm_apu_scmi`).
     - Continues to use direct register reads/writes
       (:ref:`amd_pm_rpu_direct`). RPU to PLM communication is currently in
       planning (see :ref:`amd_pm_rpu_plm`).
