# Game Screen Barrier

This program produces an executable that restricts the mouse pointer to the left screen, minus a slight indent on each side (currently 20px). It was written by and primarlily for linux users struggling with the steam port of `Kingdom Hearts 1.5+2.5 Remix`.

To compile (Linux):
```bash
gcc screen-barrier.c -o screen-barrier -lX11 -lXfixes
```

To run (Linux), add directory to path to run anywhere:
```bash
screen-barrier
```