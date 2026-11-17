Title: PTP

# PTP (IEEE 1588)

Cameras name their PTP features differently. [class@Aravis.Camera] detects
the known feature sets ("presets") and hides them behind one API:

* [method@Aravis.Camera.is_ptp_available]
* [method@Aravis.Camera.is_ptp_mode_available],
  [method@Aravis.Camera.set_ptp_mode], [method@Aravis.Camera.get_ptp_mode]
  ([enum@Aravis.PtpMode]: off, auto, slave only, master only)
* [method@Aravis.Camera.get_ptp_status]
* [method@Aravis.Camera.is_timestamp_latch_available],
  [method@Aravis.Camera.get_latched_timestamp]

The presets, tried in this order (the detected one is logged with
`ARV_DEBUG=device:2`):

| Preset | Enable | Role | Status |
|---|---|---|---|
| SFNC | `PtpEnable` | `PtpSlaveOnly` (optional) | `PtpStatus` after `PtpDataSetLatch` |
| Allied Vision PtpMode | `PtpMode` = Off, Auto, Slave, Master | same feature | `PtpStatus` |
| GigE Vision 2.0, latched status | `GevIEEE1588` | `GevIEEE1588Mode` (optional) | `GevIEEE1588StatusLatched` after `GevIEEE1588DataSetLatch` |
| GigE Vision 2.0 | `GevIEEE1588` | `GevIEEE1588Mode` (optional) | `GevIEEE1588Status` |

Modes per preset:

* SFNC: off and auto always; slave only with `PtpSlaveOnly`; no master only.
* Allied Vision: the `PtpMode` entries the camera offers.
* GigE Vision 2.0: off always; with `GevIEEE1588Mode`, auto and slave only
  (`Auto`, `SlaveOnly`), master only if the camera offers a `MasterOnly`
  entry; without it, auto only.

Devices often accept a role change only while PTP is disabled, so
[method@Aravis.Camera.set_ptp_mode] disables an enabled PTP for the change.

The timestamp latch is SFNC `TimestampLatch` / `TimestampLatchValue` (ns), or
GigE Vision `GevTimestampControlLatch` / `GevTimestampValue`, converted with
`GevTimestampTickFrequency`.

A camera can report `Slave` before its clock has been stepped (right after
PTP is enabled), so a synchronization check should compare the latched
timestamp with the host clock as well; the GStreamer element's `ptp-wait`
does.

## References

* GenICam SFNC, "PTP Control" (EMVA):
  [GenICam_SFNC_v2_7.pdf](https://www.emva.org/wp-content/uploads/GenICam_SFNC_v2_7.pdf).
* IDS: `PtpEnable`
  ([ptp enable](https://1stvision.com/cameras/IDS/IDS-manuals/en/ptp-enable.html)),
  `PtpSlaveOnly`, an IDS feature, not SFNC
  ([ptp slave only](https://1stvision.com/cameras/IDS/IDS-manuals/en/ptp-slave-only.html)),
  `PtpStatus` (SFNC; Initializing, Faulty, Disabled, Listening, PreMaster,
  Master, Passive, Uncalibrated, Slave;
  [ptp status](https://1stvision.com/cameras/IDS/IDS-manuals/en/ptp-status.html)).
* Basler: `GevIEEE1588`, `GevIEEE1588DataSetLatch`,
  `GevIEEE1588StatusLatched` (recommended over `GevIEEE1588Status`), and
  `PtpEnable` / `PtpDataSetLatch` on ace 2 and dart M:
  [Precision Time Protocol](https://docs.baslerweb.com/precision-time-protocol).
* Allied Vision: `PtpMode` (Off, Slave, Master, Auto) and `PtpStatus`:
  [PTP IEEE 1588 with Prosilica GT, GC, Manta](https://www.alliedvision.com/assets/support/Camera-Documentation/Allied-Vision/FAQs-Application-Notes/PTP_IEEE1588_with_Prosilica_GT_GC_Manta.pdf).
* Emergent Vision: its `PtpMode` (Off, OneStep, TwoStep) selects the sync
  message processing, not the role, and is not taken for the Allied Vision
  preset:
  [PtpMode](https://docs.emergentvisiontec.com/camera-features/ptpmode).
* Teledyne FLIR: `GevIEEE1588`, `GevIEEE1588Mode`, `GevIEEE1588Status`
  ([Forge transport layer control](https://softwareservices.flir.com/FG-P5G-50S4/latest/Model/public/TransportLayerControl.html));
  the GenICam XML of a Forge FG-P5G-120S6C lists `GevIEEE1588Mode` entries
  `Auto` and `SlaveOnly`, and `GevIEEE1588StatusLatched` with
  `GevIEEE1588DataSetLatch`.
