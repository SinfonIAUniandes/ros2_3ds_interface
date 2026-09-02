# Generated DDS message layer

This folder contains Cyclone DDS generated headers and source files produced by the IDL translator.

Important rules:

- Treat this content as generated output, not hand-written application logic.
- Do not edit generated files to add business logic or runtime behavior.
- Keep the application code dependent on the stable compatibility headers in the project include tree, especially [include/ros2_types.h](../include/ros2_types.h).
- Regenerate the files from the IDL source whenever the ROS message set changes.

Why this matters:

- Generated code typically embeds translator-specific naming and includes that are noisy and fragile for application code.
- A thin compatibility layer keeps the rest of the project resilient to toolchain or generator changes.
- This keeps the 3DS runtime code simpler and more reusable.
