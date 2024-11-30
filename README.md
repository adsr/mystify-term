# mystify-term

A terminal recreation of the "Mystify" screensaver[^1] introduced in
Windows 3.1.

![screencap](https://github.com/adsr/mystify-term/blob/master/screencap.gif?raw=true)

It uses the "Symbols for Legacy Computing" Unicode block[^2] to blit
six "pixels" per terminal cell.

Terminal I/O is handled by termbox2[^3].

### Build and run

```console
$ make
...
$ ./mystify-term -h
Usage:
  mystify-term [options]

Options:
  -h, --help                  Show this help
  -v, --version               Show program version
  -q, --polys=<int>           Set number of polygons (default=2, max=16)
  -p, --points=<int>          Set number of points per polygon (default=4, max=16)
  -t, --trails=<int>          Set number of trails lines (default=20, max=64)
  -f, --fps=<int>             Set frames per second (default=60)
  -e, --max-velocity=<float>  Set max velocity per point (default=1.00, min=0.01, max=10.00)
  -s, --no-status             Hide status text
  -i, --trail-incr=<int>      Render every nth trail (default=4, max=64)
$ ./mystify-term
...
```

Parameters can be adjusted at runtime.

[^1]: https://microsoft.fandom.com/wiki/Mystify_Your_Mind
[^2]: https://en.wikipedia.org/wiki/Symbols_for_Legacy_Computing
[^3]: https://github.com/termbox/termbox2