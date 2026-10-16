---
"create-lynxtron": patch
---

Fix the desktop template bridge handler typecheck by inferring the invoke event type from LynxWindow.on instead of referencing the undeclared EventCallback type.
