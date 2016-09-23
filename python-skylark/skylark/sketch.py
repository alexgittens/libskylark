import lib
import errors
import ctypes
from ctypes import byref, cdll, c_double, c_void_p, c_int, c_char_p, pointer, POINTER, c_bool
import ctypes.util
import math
from math import sqrt, pi
import numpy, scipy
import scipy.sparse
import scipy.fftpack
import sys
import os
import json

_libc = cdll.LoadLibrary(ctypes.util.find_library('c'))
_libc.free.argtypes = (ctypes.c_void_p,)

_haslib = lib.lib is not None

if _haslib:
  csketches = map(eval, lib.lib.sl_supported_sketch_transforms().split())
  pysketches = ["SJLT", "PPT", "URST", "NURST"]
  SUPPORTED_SKETCH_TRANSFORMS = \
                                csketches + [ (T, "Matrix", "Matrix") for T in pysketches]
else:      
  sketches = ["JLT", "CT", "SJLT", "FJLT", "CWT", "MMT", "WZT", "GaussianRFT",
              "FastGaussianRFT", "PPT", "URST", "NURST"]
  SUPPORTED_SKETCH_TRANSFORMS = [ (T, "Matrix", "Matrix") for T in sketches]

  # TODO seed for pure-Python implementation?
  

def deserialize_sketch(sketch_dict):
  """
  Load Serialized Transform
  :param sketch_dict dictionary that is the sketch in serialized form (from .serialize()).
  """
  sketch_transform = c_void_p()
  lib.callsl("sl_deserialize_sketch_transform", \
            json.dumps(sketch_dict), byref(sketch_transform))
  sketch_name = str(sketch_dict['sketch_type'])
  return _map_csketch_type_to_cfun[sketch_name](sketch_dict, sketch_transform)

#
# Generic Sketch Transform
#
class _SketchTransform(object):
  """  A sketching transform - in very general terms - is a dimensionality-reducing map
  from R^n to R^s which preserves key structural properties.

  _SketchTransform is base class sketch transforms. The various sketch transforms derive
  from this class and as such it defines a common interface. Derived classes can have different
  constructors. The class is not meant
  """

  def __init__(self, ttype, n, s, defouttype=None, forceppy=False, sketch_transform=None):
    """
    Create the transform from n dimensional vectors to s dimensional vectors. Here we define
    the interface, but the constructor should not be called directly by the user.

    :param ttype: String identifying the sketch type. This parameter is omitted
                  in derived classes.
    :param n: Number of dimensions in input vectors.
    :param s: Number of dimensions in output vectors.
    :param defouttype: Default output type when using the * and / operators.
                       If None the output will have same type as the input.
    :param forceppy: whether to force a pure python implementation
    :param sketch_transform: Loaded sketch transform from serialized data.
    :returns: the transform object
    """

    self._baseinit(ttype, n, s, defouttype, forceppy)
    if not self._ppy:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, ttype, n, s, byref(sketch_transform))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

  def _baseinit(self, ttype, n, s, defouttype, forceppy):
    if defouttype is not None and not lib.map_to_ctor.has_key(defouttype):
      raise errors.UnsupportedError("Unsupported default output type (%s)" % defouttype)
    self._ttype = ttype
    self._n = n
    self._s = s
    self._defouttype = defouttype
    self._ppy = (not _haslib) or forceppy

  def __del__(self):
    if not self._ppy:
      lib.callsl("sl_free_sketch_transform", self._obj)

  def serialize(self):
    """
    Returns a dictionary that is the sketch in a serialized for.
    That is, the sketch object can be reconstructed using the deserialize_sketch
    function.
    """
    if not self._ppy:
      json_data = c_char_p()
      lib.callsl("sl_serialize_sketch_transform", self._obj, byref(json_data))
      try:
        serialized_sketch = json.loads(json_data.value)
      except ValueError:
        _libc.free(json_data)
        print "Failed to parse JSON"
      else:
        _libc.free(json_data)
        return serialized_sketch
    else:
        # TODO: python serialization of sketch
        pass

  def __getstate__(self):
    d = self.__dict__.copy()
    if d.has_key("_obj"): d["_obj"] = self.serialize()
    return d

  def __setstate__(self, d):
    self.__dict__ = d
    try:
      sketch_transform = c_void_p()
      lib.callsl("sl_deserialize_sketch_transform", \
                json.dumps(d["_obj"]), byref(sketch_transform))
      self._obj = sketch_transform.value
    except:
      pass

  def apply(self, A, SA, dim=0):
    """
    Apply the transform on **A** along dimension **dim** and write
    result in **SA**. Note: for rowwise (aka right) sketching **A**
    is mapped to **A S^T**.

    :param A: Input matrix.
    :param SA: Ouptut matrix. If "None" then the output will be allocated.
    :param dim: Dimension to apply along. 0 - columnwise, 1 - rowwise.
                or can use "columnwise"/"rowwise", "left"/"right"
                default is columnwise
    :returns: SA
    """
    if dim == 0 or dim == "columnwise" or dim == "left":
      dim = 0
    if dim == "rowwise" or dim == "right":
      dim = 1
    if dim != 0 and dim != 1:
      raise ValueError("Dimension must be either columnwise/rowwise or left/right or 0/1")

    A = lib.adapt(A)

    # Allocate in case SA is not given, and then adapt it.
    if SA is None:
      if self._defouttype is None:
        ctor = A.getctor()
      else:
        ctor = lib.map_to_ctor[self._defouttype]

      if dim == 0:
        SA = ctor(self._s, A.getdim(1), A)
      if dim == 1:
        SA = ctor(A.getdim(0), self._s, A)
    SA = lib.adapt(SA)

    reqcomb = (self._ttype, A.ctype(), SA.ctype())
    if reqcomb not in SUPPORTED_SKETCH_TRANSFORMS:
      raise errors.UnsupportedError("Unsupported transform-input-output combination: " \
                                      + str(reqcomb))

    incomp, cinvert = A.iscompatible(SA)
    if incomp is not None:
      raise errors.UnsupportedError("Input and output are incompatible: " + incomp)

    if A.getdim(dim) != self._n:
      raise errors.DimensionMistmatchError("Sketched dimension is incorrect (input)")
    if SA.getdim(dim) != self._s:
      raise errors.DimensionMistmatchError("Sketched dimension is incorrect (output)")
    if A.getdim(1 - dim) != SA.getdim(1 - dim):
      raise errors.DimensionMistmatchError("Sketched dimension is incorrect (input != output)")

    if self._ppy:
      self._ppyapply(A.getobj(), SA.getobj(), dim)
    else:
      Aobj = A.ptr()
      SAobj = SA.ptr()
      if (Aobj == -1 or SAobj == -1):
        raise errors.InvalidObjectError("Invalid/unsupported object passed as A or SA")

      if cinvert:
        cdim = 1 - dim
      else:
        cdim = dim

      lib.callsl("sl_apply_sketch_transform", self._obj, \
                A.ctype(), Aobj, SA.ctype(), SAobj, cdim+1)

      A.ptrcleaner()
      SA.ptrcleaner()

    return SA.getobj()

  def __mul__(self, A):
    """
    Allocate space for **SA** and apply the transform columnwise to **A**
    writing the result to **SA** and returning it.

    :param A: Input matrix.
    :returns: the result of applying the transform to **A** columnwise.
    """
    return self.apply(A, None, dim=0)

  def __div__(self, A):
    """
    Allocate space for **SA** and apply the transform rowwise to **A**
    writing the result to **SA** and returning it.

    :param A: Input matrix.
    :returns: the result of applying the transform to **A** rowwise.
    """
    return self.apply(A, None, dim=1)

  def getindim(self):
    """
    Get size of input.
    """
    return self._n

  def getsketchdim(self):
    """
    Get dimension of sketched output.
    """
    return self._s

#
# Various sketch transforms
#

class JLT(_SketchTransform):
  """
  The classic Johnson-Lindenstrauss dense sketching using Gaussian Random maps.

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  Examples
  --------
  Let us bring *skylark* and other relevant Python packages into our environment.
  Here we demonstrate a non-distributed usage implemented using numpy arrays.
  See section on working with distributed dense and sparse matrices.

  >>> import skylark, skylark.utilities, skylark.sketch
  >>> import scipy
  >>> import numpy.random
  >>> import matplotlib.pyplot as plt

  Let us generate some data, e.g., a data matrix whose entries are sampled
  uniformly from the interval [-1, +1].

  >>> n = 300
  >>> d = 1000
  >>> A = numpy.random.uniform(-1.0,1.0, (n,d))

  Create a sketch operator corresponding to JLT sketching from d = 1000
  to s = 100.

  >>> s = 100
  >>> S = skylark.sketch.JLT(d, s)

  Let us sketch A row-wise:

  >>> B = S / A

  Let us compute norms of the row-vectors before and after sketching.

  >>> norms_A = skylark.utilities.norms(A)
  >>> norms_B = skylark.utilities.norms(B)

  Plot the histogram of distortions (ratio of norms for original to sketched
  vectors).

  >>> distortions = scipy.ravel(norms_A/norms_B)
  >>> plt.hist(distortions,10)
  >>> plt.show()
  """
  def __init__(self, n, s, defouttype=None, forceppy=False, sketch_transform=None):
    super(JLT, self).__init__("JLT", n, s, defouttype, forceppy, sketch_transform);
    if self._ppy:
      # The following is not memory efficient, but for a pure Python impl it will do
      self._S = numpy.random.standard_normal((s, n)) / sqrt(s)

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      SA1 = numpy.dot(self._S, A)
    if dim == 1:
      SA1 = numpy.dot(A, self._S.T)

    # We really want to use the out parameter of numpy.dot, but it does not seem
    # to work (raises a ValueError)
    numpy.copyto(SA, SA1)

class SJLT(_SketchTransform):
  """
  Sparse Johnson-Lindenstrauss Transform

  Alternative name: SparseJLT

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param density: Density of the transform matrix. Lower density require higher s.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *D. Achlipotas*, **Database-frinedly random projections: Johnson-Lindenstrauss
  with binary coins**, Journal of Computer and System Sciences 66 (2003) 671-687

  *P. Li*, *T. Hastie* and *K. W. Church*, **Very Sparse Random Projections**,
  KDD 2006
  """
  def __init__(self, n, s, density = 1 / 3.0, defouttype=None, forceppy=False):
    super(SJLT, self)._baseinit("SJLT", n, s, defouttype, forceppy);
    self._ppy = True
    nz_values = [-sqrt(1.0/density), +sqrt(1.0/density)]
    nz_prob_dist = [0.5, 0.5]
    self._S = scipy.sparse.rand(m, n, density, format = 'csr')
    self._S.data = scipy.stats.rv_discrete(values=(nz_values, nz_prob_dist), name = 'dist').rvs(size=S.nnz)
    # QUESTION do we need to mulitply by sqrt(1/density) ???

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      SA1 = self._S * A
    if dim == 1:
      SA1 = A * self._S.T

    # We really want to use the out parameter of numpy.dot, but it does not seem
    # to work (raises a ValueError)
    numpy.copyto(SA, SA1)

class CT(_SketchTransform):
  """
  Cauchy Transform

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param C: Parameter trading embedding size and distortion. See paper for details.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *C. Sohler* and *D. Woodruff*, **Subspace Embeddings for the L_1-norm with
  Application**, STOC 2011
  """
  def __init__(self, n, s, C, defouttype=None, forceppy=False, sketch_transform=None):
    super(CT, self)._baseinit("CT", n, s, defouttype, forceppy)

    if self._ppy:
      self._S = numpy.random.standard_cauchy((s, n)) * (C / s)
    else:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "CT", n, s, \
                  byref(sketch_transform), ctypes.c_double(C))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      SA1 = numpy.dot(self._S, A)
    if dim == 1:
      SA1 = numpy.dot(A, self._S.T)

    # We really want to use the out parameter of numpy.dot, but it does not seem
    # to work (raises a ValueError)
    numpy.copyto(SA, SA1)

class FJLT(_SketchTransform):
  """
  Fast Johnson-Lindenstrauss Transform

  Alternative class name: FastJLT

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *N. Ailon* and *B. Chazelle*, **The Fast Johnson-Lindenstrauss Transform and
  Approximate Nearest Neighbors**, SIAM Journal on Computing 39 (1), pg. 302-322
  """
  def __init__(self, n, s, defouttype=None, forceppy=False, sketch_transform=None):
    super(FJLT, self).__init__("FJLT", n, s, defouttype, forceppy, sketch_transform);
    if self._ppy:
      d = scipy.stats.rv_discrete(values=([-1,1], [0.5,0.5]), name = 'uniform').rvs(size=n)
      self._D = scipy.sparse.spdiags(d, 0, n, n)
      self._S = URST(n, s, outtype, forceppy=forceppy)

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      DA = self._D * A
      FDA = scipy.fftpack.dct(DA, axis = 0, norm = 'ortho')
      self._S.apply(FDA, SA, dim);

    if dim == 1:
      AD = A * self._D
      ADF = scipy.fftpack.dct(AD, axis = 1, norm = 'ortho')
      self._S.apply(ADF, SA, dim);

class CWT(_SketchTransform):
  """
  Clarkson-Woodruff Transform (also known as CountSketch)

  Alternative class name: CountSketch

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *K. Clarkson* and *D. Woodruff*, **Low Rank Approximation and Regression
  in Input Sparsity Time**, STOC 2013
  """
  def __init__(self, n, s, defouttype=None, forceppy=False, sketch_transform=None):
    super(CWT, self).__init__("CWT", n, s, defouttype, forceppy, sketch_transform);
    if self._ppy:
      # The following is not memory efficient, but for a pure Python impl
      # it will do
      distribution = scipy.stats.rv_discrete(values=([-1.0, +1.0], [0.5, 0.5]),
                                             name = 'dist')
      self._S = _hashmap(s, n, distribution, dimension = 0)

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      SA1 = self._S * A
    if dim == 1:
      SA1 = A * self._S.T

    # We really want to use the out parameter of scipy.dot, but it does not seem
    # to work (raises a ValueError)
    numpy.copyto(SA, SA1)

class MMT(_SketchTransform):
  """
  Meng-Mahoney Transform. A variant of CountSketch (Clarkson-Woodruff Transform)
  using for low-distrition in the L1-norm.

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *X. Meng* and *M. W. Mahoney*, **Low-distortion Subspace Embeddings in
  Input-sparsity Time and Applications to Robust Linear Regression**, STOC 2013
  """
  def __init__(self, n, s, defouttype=None, forceppy=False, sketch_transform=None):
    super(MMT, self).__init__("MMT", n, s, defouttype, forceppy, sketch_transform);
    if self._ppy:
      # The following is not memory efficient, but for a pure Python impl
      # it will do
      distribution = scipy.stats.cauchy()
      self._S = _hashmap(s, n, distribution, dimension = 0)

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      SA1 = self._S * A
    if dim == 1:
      SA1 = A * self._S.T

    # We really want to use the out parameter of scipy.dot, but it does not seem
    # to work (raises a ValueError)
    numpy.copyto(SA, SA1)

class WZT(_SketchTransform):
  """
  Woodruff-Zhang Transform. A variant of CountSketch (Clarkson-Woodruff Transform)
  using for low-distrition in Lp-norm. p is supplied as a parameter in the
  constructor.

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param p: Defines the norm for the embedding (Lp).
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *D. Woodruff* and *Q. Zhang*, **Subspace Embeddings and L_p Regression
  Using Exponential Random**, COLT 2013
  """

  class _WZTDistribution(object):
    def __init__(self, p):
      self._edist = scipy.stats.expon()
      self._bdist = scipy.stats.bernoulli(0.5)
      self._p = p

    def rvs(self, size):
      val = numpy.empty(size);
      for idx in range(0, size):
        val[idx] = (2 * self._bdist.rvs() - 1) * math.pow(self._edist.rvs(), 1/self._p)
      return val

  def __init__(self, n, s, p, defouttype=None, forceppy=False, sketch_transform=None):
    super(WZT, self)._baseinit("WZT", n, s, defouttype, forceppy)

    if self._ppy:
      # The following is not memory efficient, but for a pure Python impl
      # it will do
      distribution = WZT._WZTDistribution(p)
      self._S = _hashmap(s, n, distribution, dimension = 0)
    else:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "WZT", n, s, \
                  byref(sketch_transform), ctypes.c_double(p))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      SA1 = self._S * A
    if dim == 1:
      SA1 = A * self._S.T

    # We really want to use the out parameter of scipy.dot, but it does not seem
    # to work (raises a ValueError)
    numpy.copyto(SA, SA1)

class GaussianRFT(_SketchTransform):
  """
  Random Features Transform for the RBF Kernel.

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param sigma: bandwidth of the kernel.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *A. Rahimi* and *B. Recht*, **Random Features for Large-scale
  Kernel Machines**, NIPS 2009
  """
  def __init__(self, n, s, sigma=1.0, defouttype=None, forceppy=False, sketch_transform=None):
    super(GaussianRFT, self)._baseinit("GaussianRFT", n, s, defouttype, forceppy)

    self._sigma = sigma
    if self._ppy:
      self._T = JLT(n, s, forceppy=forceppy)
      self._b = numpy.matrix(numpy.random.uniform(0, 2 * pi, (s,1)))
    else:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "GaussianRFT", n, s, \
                   byref(sketch_transform), ctypes.c_double(sigma))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

  def _ppyapply(self, A, SA, dim):
    self._T.apply(A, SA, dim)
    if dim == 0:
      bm = self._b * numpy.ones((1, SA.shape[1]))
    if dim == 1:
      bm = numpy.ones((SA.shape[0], 1)) * self._b.T
    SA[:, :] = sqrt(2.0 / self._s) * numpy.cos(SA * (sqrt(self._s)/self._sigma) + bm)

class LaplacianRFT(_SketchTransform):
  """
  Random Features Transform for the Laplacian Kernel

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param sigma: bandwidth of the kernel.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *A. Rahimi* and *B. Recht*, **Random Features for Large-scale
  Kernel Machines**, NIPS 2009
  """
  def __init__(self, n, s, sigma=1.0, defouttype=None, forceppy=False, sketch_transform=None):
    super(LaplacianRFT, self)._baseinit("LaplacianRFT", n, s, defouttype, forceppy)

    if not self._ppy:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "LaplacianRFT", n, s, \
                  byref(sketch_transform), ctypes.c_double(sigma))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

    # TODO ppy implementation

class MaternRFT(_SketchTransform):
  """
  Random Features Transform for the Matern Kernel

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param nu: nu parameter
  :param l: l parameter
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *A. Rahimi* and *B. Recht*, **Random Features for Large-scale
  Kernel Machines**, NIPS 2009
  """
  def __init__(self, n, s, nu, l, defouttype=None, forceppy=False, sketch_transform=None):
    super(MaternRFT, self)._baseinit("MaternRFT", n, s, defouttype, forceppy)

    if not self._ppy:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "MaternRFT", n, s, \
                  byref(sketch_transform), ctypes.c_double(nu), ctypes.c_double(l))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

    # TODO ppy implementation


class GaussianQRFT(_SketchTransform):
  """
  Quasi Random Features Transform for the Guassian Kernel

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param sigma: bandwidth of the kernel.
  :param skip: how many values in the QMC sequence to skip.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *J. Yang*, *V. Sindhwani*, *H. Avron* and *M. Mahoney*,
  **Quasi-Monte Carlo Feature Maps for Shift-Invariant Kernels**
  ICML 2014
  """
  def __init__(self, n, s, sigma=1.0, skip=0, defouttype=None, forceppy=False, sketch_transform=None):
    super(GaussianQRFT, self)._baseinit("GaussianQRFT", n, s, defouttype, forceppy)

    if not self._ppy:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "GaussianQRFT", n, s, \
                  byref(sketch_transform), ctypes.c_double(sigma), ctypes.c_int(skip))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

    # TODO ppy implementation

class LaplacianQRFT(_SketchTransform):
  """
  Quasi Random Features Transform for the Laplacian Kernel

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param sigma: bandwidth of the kernel.
  :param skip: how many values in the QMC sequence to skip.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *J. Yang*, *V. Sindhwani*, *H. Avron* and *M. Mahoney*,
  **Quasi-Monte Carlo Feature Maps for Shift-Invariant Kernels**
  ICML 2014
  """
  def __init__(self, n, s, sigma=1.0, skip=0, defouttype=None, forceppy=False, sketch_transform=None):
    super(LaplacianQRFT, self)._baseinit("LaplacianQRFT", n, s, defouttype, forceppy)

    if not self._ppy:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "LaplacianQRFT", n, s, \
                  byref(sketch_transform), ctypes.c_double(sigma), c_int(skip))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

    # TODO ppy implementation

class FastGaussianRFT(_SketchTransform):
  """
  Fast variant of Random Features Transform for the RBF Kernel.

  Alternative class name: Fastfood

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param sigma: bandwidth of the kernel.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *Q. Le*, *T. Sarlos*, *A. Smola*, **Fastfood - Computing Hilbert Space
  Expansions in Loglinear Time**, ICML 2013
  """
  def __init__(self, n, s, sigma=1.0, defouttype=None, forceppy=False, sketch_transform=None):
    super(FastGaussianRFT, self)._baseinit("FastGaussianRFT", n, s, defouttype, forceppy);

    self._sigma = sigma
    if self._ppy:
      self._blocks = int(math.ceil(float(s) / n))
      self._sigma = sigma
      self._b = numpy.matrix(numpy.random.uniform(0, 2 * pi, (s,1)))
      binary = scipy.stats.bernoulli(0.5)
      self._B = [2.0 * binary.rvs(n) - 1.0 for i in range(self._blocks)]
      self._G = [numpy.random.randn(n) for i in range(self._blocks)]
      self._P = [numpy.random.permutation(n) for i in range(self._blocks)]
    else:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "FastGaussianRFT", n, s, \
                   byref(sketch_transform), ctypes.c_double(sigma))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

  def _ppyapply(self, A, SA, dim):
    blks = [self._ppyapplyblk(A, dim, i) for i in range(self._blocks)]
    SA0 = numpy.concatenate(blks, axis=dim)
    if dim == 0:
      bm = self._b * numpy.ones((1, SA.shape[1]))
      if self._s < SA0.shape[0]:
        SA0 = SA0[:self._s, :]
    if dim == 1:
      bm = numpy.ones((SA.shape[0], 1)) * self._b.T
      if self._s < SA0.shape[1]:
        SA0 = SA0[:, :self._s]
    SA[:, :] = sqrt(2.0 / self._s) * numpy.cos(SA0 / (self._sigma * sqrt(self._n)) + bm)

  def _ppyapplyblk(self, A, dim, i):
    B = scipy.sparse.spdiags(self._B[i], 0, self._n, self._n)
    G = scipy.sparse.spdiags(self._G[i], 0, self._n, self._n)
    P = self._P[i]

    if dim == 0:
      FBA = scipy.fftpack.dct(B * A, axis = 0, norm='ortho') * sqrt(self._n)
      FGPFBA = scipy.fftpack.dct(G * ABF[P, :], axis = 0, norm='ortho') * sqrt(self._n)
      return FGPFBA

    if dim == 1:
      ABF = scipy.fftpack.dct(A * B, axis = 1, norm='ortho') * sqrt(self._n)
      ABFPGF = scipy.fftpack.dct(ABF[:, P] * G, axis = 1, norm='ortho') * sqrt(self._n)
      return ABFPGF

class FastMaternRFT(_SketchTransform):
  """
  Fast variant of Random Features Transform for the Matern Kernel.

  Alternative class name: MaternFastfood

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param order: order of the kernel.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *Q. Le*, *T. Sarlos*, *A. Smola*, **Fastfood - Computing Hilbert Space
  Expansions in Loglinear Time**, ICML 2013
  """
  def __init__(self, n, s,nu, l, defouttype=None, forceppy=False, sketch_transform=None):
    super(FastMaternRFT, self)._baseinit("FastMaternRFT", n, s, defouttype, forceppy);

    self._nu = nu
    self._l = l
    if not self._ppy:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "FastMaternRFT", n, s, \
                  byref(sketch_transform), ctypes.c_double(nu), ctypes.c_double(l))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

    # TODO ppy implementation

class ExpSemigroupRLT(_SketchTransform):
  """
  Random Features Transform for the Exponential Semigroup Kernel.

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param beta: kernel parameter
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *J. Yang*, *V. Sindhwani*, *Q. Fan*, *H. Avron*, *M. Mahoney*,
  **Random Laplace Feature Maps for Semigroup Kernels on Histograms**, CVPR 2014
  """
  def __init__(self, n, s, beta=1.0, defouttype=None, forceppy=False, sketch_transform=None):
    super(ExpSemigroupRLT, self)._baseinit("ExpSemigroupRLT", n, s, defouttype, forceppy)

    self._beta = beta
    if not self._ppy:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "ExpSemigroupRLT", n, s, \
                  byref(sketch_transform), ctypes.c_double(beta))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

    # TODO ppy implementation

class ExpSemigroupQRLT(_SketchTransform):
  """
  Quasi Random Features Transform for the Exponential Semigroup Kernel.

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param beta: kernel parameter
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  **Random Laplace Feature Maps for Semigroup Kernels on Histograms**, 2014
  """
  def __init__(self, n, s, beta=1.0, defouttype=None, forceppy=False, sketch_transform=None):
    super(ExpSemigroupQRLT, self)._baseinit("ExpSemigroupQRLT", n, s, defouttype, forceppy)

    self._beta = beta
    if not self._ppy:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "ExpSemigroupQRLT", n, s, \
                  byref(sketch_transform), ctypes.c_double(beta))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

    # TODO ppy implementation

class PPT(_SketchTransform):
  """
  Pham-Pagh Transform - features sketching for the polynomial kernel.

  Alternative class name: TensorSketch

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param q: degree of kernel
  :param c: kernel parameter.
  :param gamma: normalization coefficient.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation

  *N. Pham* and *R. Pagh*, **Fast and Scalable Polynomial Kernels via Explicit
  Feature Maps**, KDD 2013
  """
  def __init__(self, n, s, q=3,  c=0, gamma=1, defouttype=None, forceppy=False, sketch_transform=None):
    super(PPT, self)._baseinit("PPT", n, s, defouttype, forceppy);

    if c < 0:
      raise ValueError("c parameter must be >= 0")

    if self._ppy:
      self._q = q
      self._gamma = gamma
      self._c = c
      self._css = [CWT(n + (c > 0), s, forceppy=forceppy) for i in range(q)]
    else:
      if sketch_transform is None:
        sketch_transform = c_void_p()
        lib.callsl("sl_create_sketch_transform", lib.ctxt_obj, "PPT", n, s, \
                   byref(sketch_transform), \
                   ctypes.c_int(q), ctypes.c_double(c), ctypes.c_double(gamma))
        self._obj = sketch_transform
      else:
        self._obj = sketch_transform

  def _ppyapply(self, A, SA, dim):
    sc = sqrt(self._c)
    sg = sqrt(self._gamma);
    if self._c != 0:
      if dim == 0:
        A = numpy.concatenate((A, (sc/sg) * numpy.ones((1, A.shape[1]))))
      else:
        A = numpy.concatenate((A, (sc/sg) * numpy.ones((A.shape[0], 1))), 1)

    P = numpy.ones(SA.shape)
    s = self._s
    for i in range(self._q):
      self._css[i].apply(sg * A, SA, dim)
      P = numpy.multiply(P, numpy.fft.fft(SA, axis=dim))
    numpy.copyto(SA, numpy.fft.ifft(P, axis=dim).real)

class URST(_SketchTransform):
  """
  Uniform Random Sampling Transform
  For now, only Pure Python implementation, and only sampling with replacement.

  Alternative class name: UniformSampler

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation
  """
  def __init__(self, n, s, defouttype=None, forceppy=False):
    super(URST, self)._baseinit("URST", n, s, defouttype, forceppy);
    self._ppy = True
    self._idxs = numpy.random.permutation(n)[0:s]

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      SA[:, :] = A[self._idxs, :]
    if dim == 1:
      SA[:, :] = A[:, self._idxs]

class NURST(_SketchTransform):
  """
  Non-Uniform Random Sampling Transform
  For now, only Pure Python implementation, and only sampling with replacement.

  Alternative class name: NonUniformSampler

  :param n: Number of dimensions in input vectors.
  :param s: Number of dimensions in output vectors.
  :param p: Probability distribution on the n rows.
  :param defouttype: Default output type when using the * and / operators.
  :param forceppy: whether to force a pure python implementation
  """
  def __init__(self, n, s, p, defouttype=None, forceppy=False):
    super(NURST, self)._baseinit("NURST", n, s, defouttype, forceppy);
    if p.shape[0] != n:
      raise errors.InvalidParamterError("size of probability array should be exactly n")
    self._ppy = True
    self._idxs = scipy.stats.rv_discrete(values=(numpy.arange(0,n), p), \
                                         name = 'uniform').rvs(size=s)

  def _ppyapply(self, A, SA, dim):
    if dim == 0:
      SA[:, :] = A[self._idxs, :]
    if dim == 1:
      SA[:, :] = A[:, self._idxs]

#
# Helper functions
#
def _hashmap(t, n, distribution, dimension=0): 
  """
  Sparse matrix representation of a random hash map h:[n] -> [t] so that 
  for each i in [n], h(i) = j for j drawn from distribution
  
  :param t: number of bins
  :param n: number of items hashed
  :param distribution: distribution object. Needs to implement the 
                       rvs(size=n) as returns an array of n samples. 
  :param dimension: 0 returns t x n matrix, 1 returns n x t matrix 
                   (for efficiency later)
  """
    
  data = distribution.rvs(size=n)
  col = scipy.arange(n)
  row = scipy.stats.randint(0,t).rvs(n)
  if dimension==0:
    S = scipy.sparse.csr_matrix( (data, (row, col)), shape = (t,n))
  else:
    S = scipy.sparse.csr_matrix( (data, (col, row)), shape = (n,t))
        
  return S

#
# Additional names for various transforms.
#
SparseJLT = SJLT
FastJLT = JLT
CountSketch = CWT
RRT = GaussianRFT
Fastfood=FastGaussianRFT
MaternFastfood=FastMaternRFT
TensorSketch = PPT
UniformSampler = URST
NonUniformSampler = NURST

#
# Mapping between serialize type string and Python calss
#
_map_csketch_type_to_cfun = { }
_map_csketch_type_to_cfun["JLT"] = lambda sd, obj : JLT(int(sd['N']), int(sd['S']), None, False, obj)
_map_csketch_type_to_cfun["CT"] = \
    lambda sd, obj : CT(int(sd['N']), int(sd['S']), float(sd['C']), None, False, obj)
_map_csketch_type_to_cfun["FJLT"] = lambda sd, obj : FJLT(int(sd['N']), int(sd['S']), None, False, obj)
_map_csketch_type_to_cfun["CWT"] = lambda sd, obj : CWT(int(sd['N']), int(sd['S']), None, False, obj)
_map_csketch_type_to_cfun["MMT"] = lambda sd, obj : MMT(int(sd['N']), int(sd['S']), None, False, obj)
_map_csketch_type_to_cfun["WZT"] = \
    lambda sd, obj : WZT(int(sd['N']), int(sd['S']), float(sd['P']), None, False, obj)
_map_csketch_type_to_cfun["GaussianRFT"] = \
    lambda sd, obj : GaussianRFT(int(sd['N']), int(sd['S']), float(sd['sigma']), None, False, obj)
_map_csketch_type_to_cfun["LaplacianRFT"] = \
    lambda sd, obj : LaplacianRFT(int(sd['N']), int(sd['S']), float(sd['sigma']), None, False, obj)
_map_csketch_type_to_cfun["MaternRFT"] = \
    lambda sd, obj : MaternRFT(int(sd['N']), int(sd['S']), float(sd['nu']), float(sd['l']), None, False, obj)
_map_csketch_type_to_cfun["GaussianQRFT"] = \
    lambda sd, obj : GaussianQRFT(int(sd['N']), int(sd['S']), float(sd['sigma']), int(sd['skip']), None, False, obj)
_map_csketch_type_to_cfun["LaplacianQRFT"] = \
    lambda sd, obj : LaplacianQRFT(int(sd['N']), int(sd['S']), float(sd['sigma']), int(sd['skip']), None, False, obj)
_map_csketch_type_to_cfun["FastGaussianRFT"] = \
    lambda sd, obj : FastGaussianRFT(int(sd['N']), int(sd['S']), float(sd['sigma']), None, False, obj)
_map_csketch_type_to_cfun["FastMaternRFT"] = \
    lambda sd, obj : FastMaternRFT(int(sd['N']), int(sd['S']), float(sd['nu']), float(sd['l']), None, False, obj)
_map_csketch_type_to_cfun["ExpSemigroupRLT"] = \
    lambda sd, obj : ExpSemigroupRLT(int(sd['N']), int(sd['S']), float(sd['beta']), None, False, obj)
_map_csketch_type_to_cfun["ExpSemigroupQRLT"] = \
    lambda sd, obj : ExpSemigroupRLT(int(sd['N']), int(sd['S']), float(sd['beta']), None, False, obj)
_map_csketch_type_to_cfun["PPT"] = \
    lambda sd, obj : PPT(int(sd['N']), int(sd['S']), int(sd['q']), float(sd['c']), float(sd['gamma']), \
                           None, False, obj)

