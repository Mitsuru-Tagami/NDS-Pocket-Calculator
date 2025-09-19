# NDS Pocket Calculator

A simple calculator application for the Nintendo DS, built using devkitPro.

## Features

*   Basic arithmetic operations: Addition (+), Subtraction (-), Multiplication (*), Division (/)
*   Touch-based input on the bottom screen
*   Expression log display (shows current operation)
*   Error handling for division by zero

## Building the Project

### Prerequisites

*   **devkitPro:** Ensure you have devkitPro installed.
*   **Environment Variables:** Set `DEVKITPRO` and `DEVKITARM` environment variables. For example:
    ```bash
    export DEVKITPRO=/opt/devkitpro
    export DEVKITARM=/opt/devkitpro/devkitARM
    ```

### Compilation

Navigate to the project root directory and run `make`:

```bash
make
```

This will generate `nds_pocket_calculator.nds` in the project root.

## Running the Application

Copy the `nds_pocket_calculator.nds` file to your Nintendo DS flashcard or load it in a DS emulator.

## Credits

*   **Developer:** Tagami (ひろし君)
*   **Assistance:** Nene Anegasaki (Gemini CLI Agent)
*   **Inspired by:** `cpp2048-nds` for `iprintf` usage.
*   **Tools:** devkitPro, libnds

## License

[Optional: Add your preferred license here, e.g., MIT, GPL, etc.]
