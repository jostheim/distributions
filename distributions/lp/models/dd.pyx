cimport _dd
import _dd

from distributions.mixins import GroupIoMixin, SharedIoMixin


NAME = 'DirichletDiscrete'
EXAMPLES = [
    {
        'shared': {'alphas': [1.0, 4.0]},
        'values': [0, 1, 1, 1, 1, 0, 1],
    },
    {
        'shared': {'alphas': [0.5, 0.5, 0.5, 0.5]},
        'values': [0, 1, 0, 2, 0, 1, 0],
    },
]
Value = int


cdef class _Shared(_dd.Shared):
    def load(self, raw):
        alphas = raw['alphas']
        cdef int dim = len(alphas)
        self.ptr.dim = dim
        cdef int i
        for i in xrange(dim):
            self.ptr.alphas[i] = float(alphas[i])

    def dump(self):
        alphas = []
        cdef int i
        for i in xrange(self.ptr.dim):
            alphas.append(float(self.ptr.alphas[i]))
        return {'alphas': alphas}

    def load_protobuf(self, message):
        cdef int dim = len(message.alphas)
        self.ptr.dim = dim
        cdef int i
        for i in xrange(self.ptr.dim):
            self.ptr.alphas[i] = message.alphas[i]

    def dump_protobuf(self, message):
        message.Clear()
        cdef int i
        for i in xrange(self.ptr.dim):
            message.alphas.append(float(self.ptr.alphas[i]))


class Shared(_Shared, SharedIoMixin):
    pass


cdef class _Group(_dd.Group):
    cdef int dim  # only required for dumping

    def __cinit__(self):
        self.dim = 0

    def load(self, dict raw):
        counts = raw['counts']
        self.dim = len(counts)
        self.ptr.count_sum = 0
        cdef int i
        for i in xrange(self.dim):
            self.ptr.count_sum += counts[i]
            self.ptr.counts[i] = counts[i]

    def dump(self):
        counts = []
        cdef int i
        for i in xrange(self.dim):
            counts.append(self.ptr.counts[i])
        return {'counts': counts}

    def init(self, _dd.Shared shared):
        self.dim = shared.ptr.dim
        _dd.Group.init(self, shared)


class Group(_Group, GroupIoMixin):
    pass


class Sampler(_dd.Sampler):
    pass


Mixture = _dd.Mixture
sample_value = _dd.sample_value
sample_group = _dd.sample_group
