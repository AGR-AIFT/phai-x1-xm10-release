# Advanced Topics

A starting point for self-directed learning. This page gives you a bird's-eye view of which examples to tackle first based on your area of interest. Skim Examples 16, 18, and 19 along with the API Reference before diving in — that will give you the right mental model.

---

## Jump Right In with Examples

The advanced topics below have hands-on examples ready to run in the `examples/` folder.

| Topic | Example | Description |
| :--- | :--- | :--- |
| **TinyML on STM32** | [Ex.16 — Tiny AI Sensor Fusion](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) | Complementary filter + 3-layer NN inference on MCU |
| **Inverted-Pendulum Gait Control** | [Ex.15 — Inverted Pendulum](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/15_Inverted_Pendulum_Control/) | MgL·sin(θ) gravity compensation + PD stabilization |
| **FSM Gait Intent Recognition** | [Ex.17 — FSM Gait Intent](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/17_FSM_Gait_Intent/) | 7-phase gait FSM + phase-specific assist torque |
| **PD Control Theory** | [Ex.14 — PD Realtime Control](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/14_PD_Realtime_Control/) | Discrete-time PD control implemented from first principles |
| **System Debugging** | [Ex.18 — Debug Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/) | Loop profiling + health dashboard |
| **Memory Patterns** | [Ex.19 — Memory Aware Design](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/19_Memory_Aware_Design/) | Ring buffer, pool allocator, moving average |

---

Content requests and contributions are welcome via [GitHub Issues](https://github.com/AGR-AIFT/phai-x1-xm10-release/issues).

---

## Recommended Self-Directed Learning Paths

| Area of Interest | Recommended Starting Point | Next Steps |
|----------|-----------|------|
| **AI / ML on MCU** | [Ex.16 TinyAI Sensor Fusion](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/16_TinyAI_Sensor_Fusion/) | [Ex.36 On-Device Kinesthetic Learning](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) → [AI Training Data Pipeline](ai-data-pipeline.md) |
| **Gait Control Algorithms** | [Ex.15 Inverted Pendulum](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/15_Inverted_Pendulum_Control/) → [Ex.17 FSM Gait](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/17_FSM_Gait_Intent/) | [Ex.20–31 Algorithm Series](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/20_Impedance_Control/) |
| **Disturbance Observer + Transparent Mode** | [Ex.21 Gravity Comp](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/21_Gravity_Compensation/) | [Ex.31 DOB Stage 1](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/31_Friction_Comp_DOB/) → [Ex.35 MultiLayer](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/35_MultiLayer_Transparent_Control/) |
| **Learning / Adaptive Control** | [Ex.26 ILC](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/26_Iterative_Learning_Control/) | [Ex.27 MRAC](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/27_MRAC/) → [Ex.33 Kinesthetic Teaching](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/33_Kinesthetic_Teaching/) |
| **Data Collection / Analysis** | [Ex.09 CDC Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) | → [AI Training Data Pipeline](ai-data-pipeline.md) |
| **System Diagnostics / Debugging** | [Ex.18 Debug Monitor](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/18_Debug_Monitor/) | [Ex.19 Memory Aware Design](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/19_Memory_Aware_Design/) |

If you get stuck, check [docs/troubleshooting.md](../troubleshooting.md) or tell Claude Code "I'm stuck on Ex.XX".
