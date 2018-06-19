
Playing around with data generated from [Project-COGSWORTH].

The `viz` tool takes the printf-like pattern of the filename, width and
height, then reads the data and outputs a pnm-image on stdout.

```
Usage: ./viz <filename-pattern> <width> <height>
Output is written to stdout
Example:
./viz SAMPLE_3162099_%03d-%03d.dmp 200 70 > foo.pnm
```

Here a concrete run with data provided by Paul:
```
make
./viz /tmp/3162099/SAMPLE_3162099_%d_%d.dmp 200 70 | pnmtopng > /tmp/foulab.png
timg /tmp/foulab.png
```

Note, there are four colormaps in [colormaps.h](./colormaps.h) (kMagmaColors,
kInfernoColors, kPlasmaColors, kViridisColors).

[play around with them](https://github.com/hzeller/cogsworth-viz/blob/master/viz.cc#L17) to see which looks best (this should be a command line option...).

Useful is the [timg] tool to visuzalize on the shell.

<img src="./img/foulab.png" width="100%"/>

[Project-COGSWORTH]: https://github.com/FOULAB/Project-COGSWORTH/
[timg]: https://github.com/hzeller/timg