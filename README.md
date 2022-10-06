# Goals
This may one day become a text editor, mainly for source code. Its main goals are performance, configurability and extensibility.

To achieve the desired performance, the editor code will be written as low-level and simple as is feasible. At least in the beginning, it will exist run only on Windows and be built directly on top of Win32, so we can forego the many bloated layers of dependencies that most of today's software uses. Since this is also an educational project for me, even a lot of functionality provided by Win32 will be ignored and instead built by hand.

Configuration will be somewhat inspired by Emacs, consisting of variables and functions, for which key combinations can be mapped.

I'm personally not a fan of the fact that many editor devs feel the need to invent their own scripting languages for extensibility. For this editor, I'm planning to use a plug-in system directly at the C-level.

# Current Progress
## Font File Parser
In the beginning, only TTF fonts using Unicode BMP encoding will be supported. I'm currently using Microsoft's Consolas font for testing.

Glyph contour points for a unicode letter can now be retrieved and rendered.

# Next Steps
- Glyph rasterization
- Text buffer
- Text file loading (at first UTF-8 only)
