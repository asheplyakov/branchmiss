
import numpy as np
from timeit import timeit


N = 5000
LOOPS = 100000

a = np.arange(1, N+1)
tOrdFull = timeit(lambda: a.sum(), number=LOOPS)
tOrdPart = timeit(lambda: a.sum(where=(a>=N//2)), number=LOOPS)
print("ordered all     : {1}+{2}+...+{3}     : {0:0.3f} sec".format(tOrdFull, a[0], a[1], a[-1]))
print("ordered partial : >= {1:d}          : {0:0.3f} sec".format(tOrdPart, N//2))

r = np.arange(1, N+1)
np.random.shuffle(r)
tRandFull = timeit(lambda: r.sum(), number=LOOPS)
tRandPart = timeit(lambda: r.sum(where=(r>=N//2)), number=LOOPS)
print("shuffled all    : {1}+{2}+...+{3}: {0:0.3f} sec".format(tRandFull, r[0], r[1], r[-1]))
print("shuffled partial: >= {1}          : {0:0.3f} sec".format(tRandPart, N//2))
