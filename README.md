# ![](blob:https://www.messenger.com/bca21998-fade-4398-a7ec-cbe599d99496)STM32 Stack 3D

A Stack game and software 3D renderer for the STM32F429ZITx. The firmware
renders directly to a 320 x 240 ILI9341 display over SPI5 and uses the PA0
user button as its only input.

## Demo

[![Stack 3D gameplay on an ILI9341 display](docs/media/stack-3d-demo-cover.png)](docs/media/stack-3d-demo.mp4)

Click the image to open the gameplay video.

## Gameplay

- A block moves along the X or Z axis. The movement axis alternates after
  every successful placement.
- Pressing PA0 places the moving block.
- The overlap remains on the tower. The overhanging section is split into a
  falling piece and accelerated downward by gravity.
- A placement with no overlap ends the game and drops the active block.
- Speed starts at `1.2 units/s`, increases by `0.08 units/s` per point, and is
  capped at `3.0 units/s`.
- The renderer keeps the latest 20 settled blocks and up to four falling
  pieces in memory.
- The best score remains available until the MCU is reset or loses power.

## Controls

| State     | PA0 user button        |
| --------- | ---------------------- |
| Title     | Start the game         |
| Playing   | Place the moving block |
| Game over | Restart the game       |

PA0 is active-high and handled through EXTI0 with a 50 ms debounce interval.
Each accepted edge creates one input event; there is no double-click delay.

## Renderer

The project includes a CPU rasterizer written in C for the STM32F4:

- model, view, and perspective projection matrices;
- isometric camera tracking through a look-at matrix;
- multi-object rendering against one shared Z-buffer;
- triangle back-face culling and depth-tested occlusion;
- flat per-face lighting in RGB565;
- chunked LCD output with a HUD overlay;
- optional wireframe and coordinate-axis rendering outside gameplay;
- static raster scratch buffers with no per-frame heap allocation.

Gameplay renders cube instances with independent world matrices and colors.
The HUD is composited into each output chunk before it is sent to the LCD,
which avoids clearing and redrawing the UI as a separate frame.

## Hardware

| Function         | Connection                    |
| ---------------- | ----------------------------- |
| MCU              | STM32F429ZIT6 / STM32F429ZITx |
| Display          | ILI9341, 320 x 240, landscape |
| SPI clock        | PF7 / SPI5_SCK                |
| SPI data         | PF9 / SPI5_MOSI               |
| LCD chip select  | PC2                           |
| LCD data/command | PD13                          |
| LCD reset        | PD12                          |
| User button      | PA0 / EXTI0, active-high      |

SPI5 runs as the display master. The current CubeMX configuration reports a
45 Mbit/s SPI baud rate and uses DMA2 Stream 4 for SPI5 TX.

## Architecture

```text
PA0 EXTI event -----> main loop -----> stack_game
                         |                 |
                         |                 +-- placement, overlap, gravity
                         |                 +-- score and camera state
                         v
                   graphics_3d -----> RGB565 chunks -----> ILI9341
                         ^
                         |
                      HUD overlay
```

The main loop consumes button events, calculates delta time, updates the game,
and renders the next frame. `stack_game` owns gameplay state and produces the
render objects. `graphics_3d` transforms and rasterizes those objects before
the ILI9341 driver transfers the completed chunks.

## Project Layout

```text
Core/Inc/stack_game.h        Game state and public game API
Core/Src/stack_game.c        Placement, cropping, falling pieces, camera, HUD
Core/Inc/graphics_3d.h       Renderer types, matrix math, mesh API
Core/Src/graphics_3d.c       Software rasterizer and primitive mesh generation
Core/Inc/graphics_2d.h       Display dimensions and 2D helpers
Core/Src/main.c              HAL setup, PA0 events, update/render loop
Core/Inc/ILI9341_*            LCD driver and drawing API
Core/Src/ILI9341_*            LCD driver implementation
stm3d.ioc                    STM32CubeMX peripheral configuration
stm3d.launch                 STM32CubeIDE debug configuration
```

## Build and Flash

### Requirements

- STM32CubeIDE with the GNU Tools for STM32 toolchain;
- an ST-LINK or compatible programmer/debug probe;
- an STM32F429ZITx target wired to the ILI9341 as listed above.

### Steps

1. Open STM32CubeIDE.
2. Select **File > Import > General > Existing Projects into Workspace**.
3. Choose this repository as the project root and import `stm3d`.
4. Select the `Debug` build configuration.
5. Run **Project > Build Project**. The ELF output is
   `Debug/stm3d.elf`.
6. Connect the target through ST-LINK.
7. Use **Run > Debug As > STM32 C/C++ Application**. The included
   `stm3d.launch` configuration loads the Debug ELF.

## Current Limits

- The touch controller is not used; the LCD only displays the game and HUD.
- There is no audio output.
- The best score is not stored in flash.
- There is no perfect-placement bonus.
- The UI targets a fixed 320 x 240 landscape display.
- Wireframe and coordinate axes are disabled during gameplay.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
