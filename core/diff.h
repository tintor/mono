#pragma once
#include "core/tensor.h"
#include "core/util.h"
#include "core/algorithm.h"
#include "core/random.h"

// Stages of Diff graph:
// - construction - final size of tensors is known
// - optimization
// - Forward(bool) - v tensors populated
// - ResetGradients() - g tensors cleared
// - Backward() - g tensors accumulated
// - Optimize() - v tensors adjusted using g tensors

struct DiffT;

using Diff = std::shared_ptr<DiffT>;

struct DiffT {
    virtual ~DiffT() {}
    virtual std::vector<Diff> Inputs() { return {}; }

    static thread_local bool has_overload;
    virtual void BeginEpoch(bool training) { has_overload = false; }
    virtual void Forward(bool training) { has_overload = false; }
    virtual void Backward() { has_overload = false; }
    virtual void EndEpoch(bool training) { has_overload = false; }

    void SetBatchSize(uint batch) {
        if (batched()) {
            const dim4 s = v.shape().set(0, batch, 'b');
            v.reshape(s);
            if (g) g.reshape(s);
        }
    }

    void Reshape(dim4 s) {
        v.reshape(s);
        g.reshape(s);
    }

    operator std::string() const { return fmt::format("{} {}", TypeName(*this), v.shape()); }

    dim_t dim(uint i) const { return v.shape()[i]; }
    auto shape() const { return v.shape(); }
    auto ndims() const { return v.ndims(); }
    auto elements() const { return v.elements(); }
    bool batched() const { return ndims() > 0 && shape().name(0) == 'b'; }

    bool inference = true;
    std::string name;
    vtensor v, g;

    ulong forward_ticks = 0;
    ulong backward_ticks = 0;
};

inline Diff operator<<(Diff a, std::string_view name) {
    a->name = name;
    return a;
}

#define EACH(V) FOR(i, V.elements())

#define Declare1(Func) \
    inline Diff Func(Diff a) { return std::make_shared<Func##T>(a); }

struct Diff1 : public DiffT {
    Diff1(Diff a) : a(a), va(a->v), ga(a->g) {}
    std::vector<Diff> Inputs() override { return {a}; }
    Diff a;
    vtensor& va;
    vtensor& ga;
};

struct Diff2 : public Diff1 {
    Diff2(Diff a, Diff b) : Diff1(a), b(b), vb(b->v), gb(b->g) {}
    std::vector<Diff> Inputs() override { return {a, b}; }
    Diff b;
    vtensor& vb;
    vtensor& gb;
};

struct Diff3 : public Diff2 {
    Diff3(Diff a, Diff b, Diff c) : Diff2(a, b), c(c), vc(c->v), gc(c->g) {}
    std::vector<Diff> Inputs() override { return {a, b, c}; }
    Diff c;
    vtensor& vc;
    vtensor& gc;
};

struct DiffA : public Diff1 {
    DiffA(Diff a) : Diff1(a) { Reshape(a->shape()); }
};

struct Diff_vv : public Diff2 {
    Diff_vv(Diff a, Diff b) : Diff2(a, b) {
        Check(a->shape() == b->shape(), fmt::format("{} vs {}", a->shape(), b->shape()));
        Reshape(a->shape());
    }
};

struct Diff_sv : public Diff2 {
    Diff_sv(Diff a, Diff b) : Diff2(a, b) {
        Check(a->elements() == 1, a->shape().str());
        Reshape(b->shape());
    }
};

struct Diff_vs : public Diff2 {
    Diff_vs(Diff a, Diff b) : Diff2(a, b) {
        Check(b->elements() == 1, b->shape().str());
        Reshape(a->shape());
    }
};

bool IsBroadcastable(dim4 a, dim4 b);
Diff Broadcast(Diff a, dim4 b);

// Ignores gradients and batches.
inline Diff Const(tensor::type c) {
    auto p = std::make_shared<DiffT>();
    p->v.reshape({});
    p->v[0] = c;
    return p;
}

inline Diff Const(dim4 shape, tensor::type c) {
    auto p = std::make_shared<DiffT>();
    p->v.reshape(shape, c);
    return p;
}

inline Diff Const(const tensor c) {
    auto p = std::make_shared<DiffT>();
    p->v = c;
    return p;
}

inline bool IsConst(Diff a) { return IsType<DiffT>(a) && !a->g; }

struct Init {
    virtual ~Init() {}
    virtual tensor::type get() { return 0; }
};

struct NormalInit : public Init {
    NormalInit(tensor::type variance, RandomEngine& random) : dis(0, variance), random(random) {}
    tensor::type get() override { return dis(random); }

    std::normal_distribution<tensor::type> dis;
    std::mt19937_64& random;
};

struct UniformInit : public Init {
    UniformInit(tensor::type min, tensor::type max, RandomEngine& random) : dis(min, max), random(random) {}
    tensor::type get() override { return dis(random); }

    std::uniform_real_distribution<tensor::type> dis;
    RandomEngine& random;
};

struct ParamT : public DiffT {};

// Accepts gradients, ignores batches. Learnable parameter, can be saved.
inline Diff Param(dim4 shape, std::shared_ptr<Init> init = nullptr) {
    auto p = std::make_shared<ParamT>();
    p->Reshape(shape);
    if (init) EACH(p->v) p->v[i] = init->get();
    return p;
}

inline Diff Param(dim4 shape, tensor::type init) {
    auto p = std::make_shared<ParamT>();
    p->v.reshape(shape, init);
    p->g.reshape(shape);
    return p;
}

// Input and reference value. Not saved.
// Ignores gradients, requires batches.
inline Diff Data(dim4 shape) {
    auto p = std::make_shared<DiffT>();
    p->v.reshape(shape.push_front(1, 'b'));
    return p;
}

struct GaussianT : public DiffT {
    GaussianT(dim4 shape, tensor::type mean, tensor::type stdev, size_t seed) : normal(mean, stdev), random(seed) {
        v.reshape(shape);
    }
    void Forward(bool) override { EACH(v) v[i] = normal(random); }
    std::normal_distribution<tensor::type> normal;
    std::mt19937_64 random;
};

inline Diff Gaussian(dim4 shape, tensor::type mean, tensor::type stdev, size_t seed) {
    return std::make_shared<GaussianT>(shape, mean, stdev, seed);
}

struct ReluT : public DiffA {
    ReluT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = (va[i] > 0) ? va[i] : 0; }
    void Backward() override { EACH(ga) ga[i] += (va[i] > 0) * g[i]; }
};
Declare1(Relu);

Diff operator+(Diff a, Diff b);
inline Diff NoisyRelu(Diff a, size_t seed) { return Relu(a + Gaussian(a->shape(), 0, 1, seed)); }

// SmoothRelu
struct SoftplusT : public DiffA {
    SoftplusT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = log(1 + std::exp(va[i])); }
    void Backward() override { EACH(ga) ga[i] += g[i] / (1 + exp(-va[i])); }
};
Declare1(Softplus);

struct LeakyReluT : public DiffA {
    LeakyReluT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = (va[i] > 0) ? va[i] : (va[i] / 100); }
    void Backward() override { EACH(ga) ga[i] += (va[i] > 0) ? g[i] : (g[i] / 100); }
};
Declare1(LeakyRelu);

struct ParametricReluT : public Diff_vv {
    ParametricReluT(Diff a, Diff b) : Diff_vv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = (va[i] > 0) ? va[i] : (va[i] * vb[i]); }
    void Backward() override {
        EACH(ga) ga[i] += (va[i] > 0) ? g[i] : (g[i] * vb[i]);
        EACH(gb) gb[i] += (va[i] > 0) ? 0 : (g[i] * va[i]);
    }
};
inline Diff ParametricRelu(Diff a, Diff b) { return std::make_shared<ParametricReluT>(a, b); }

struct ELUT : public DiffA {
    ELUT(Diff a, double k) : DiffA(a), k(k) {}
    void Forward(bool) override { EACH(v) v[i] = (va[i] > 0) ? va[i] : (k * (std::exp(va[i] - 1))); }
    void Backward() override { Check(false, "not implemented"); }
    double k;
};
inline Diff ELU(Diff a, double k) { return std::make_shared<ELUT>(a, k); }

struct LogisticT : public DiffA {
    LogisticT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = 1 / (1 + std::exp(-va[i])); }
    void Backward() override { EACH(ga) ga[i] += g[i] * v[i] * (1 - v[i]); }
};
Declare1(Logistic);

struct ClampLogisticT : public DiffA {
    ClampLogisticT(Diff a, tensor::type limit) : DiffA(a), limit(limit) {}
    void Forward(bool) override { EACH(v) v[i] = 1 / (1 + std::exp(std::clamp(-va[i], -limit, limit))); }
    void Backward() override { EACH(ga) ga[i] += g[i] * v[i] * (1 - v[i]); }
    tensor::type limit;
};

inline Diff Logistic(Diff a, float limit) { return std::make_shared<ClampLogisticT>(a, limit); }

struct TanhT : public DiffA {
    TanhT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::tanh(va[i]); }
    void Backward() override { EACH(ga) ga[i] += g[i] * (1 - sqr(v[i])); }
};
Declare1(Tanh);

struct ErfT : public DiffA {
    ErfT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::erf(va[i]); }
    void Backward() override { EACH(ga) ga[i] += g[i] * tensor::type(2 / sqrt(PI)) * std::exp(-sqr(va[i])); }
};
Declare1(Erf);

// TODO variation: atan(pi*x/2)*2/pi
struct AtanT : public DiffA {
    AtanT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::atan(va[i]); }
    void Backward() override { EACH(ga) ga[i] += g[i] / (1 + sqr(va[i])); }
};
Declare1(Atan);

// x / (1 + abs(x))
struct SaxT : public DiffA {
    SaxT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] / (1 + std::abs(va[i])); }
    void Backward() override { EACH(ga) ga[i] += g[i] / sqr(1 + std::abs(va[i])); }
};
Declare1(Sax);

// 1/sqrt(1+x^2)
struct SoxT : public DiffA {
    SoxT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = 1 / std::sqrt(1 + sqr(va[i])); }
    void Backward() override { EACH(ga) ga[i] -= g[i] * va[i] * cube(v[i]); }
};
Declare1(Sox);

struct SqrT : public DiffA {
    SqrT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] * va[i]; }
    void Backward() override { EACH(ga) ga[i] += g[i] * 2 * va[i]; }
};
Declare1(Sqr);

struct SqrtT : public DiffA {
    SqrtT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::sqrt(va[i]); }
    void Backward() override { EACH(ga) ga[i] += g[i] / 2 / v[i]; }
};
Declare1(Sqrt);

struct ExpT : public DiffA {
    ExpT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::exp(va[i]); }
    void Backward() override { EACH(ga) ga[i] += g[i] * v[i]; }
};
Declare1(Exp);

struct LogT : public DiffA {
    LogT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::log(va[i]); }
    void Backward() override { EACH(ga) ga[i] += g[i] / va[i]; }
};
Declare1(Log);

Diff Pow(Diff a, Diff b);

struct AbsT : public DiffA {
    AbsT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::abs(va[i]); }
    void Backward() override { EACH(ga) ga[i] += std::copysign(g[i], va[i]); }
};
Declare1(Abs);

struct CosT : public DiffA {
    CosT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::cos(va[i]); }
    void Backward() override { EACH(ga) ga[i] -= std::sin(va[i]); }
};
Declare1(Cos);

struct SinT : public DiffA {
    SinT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = std::sin(va[i]); }
    void Backward() override { EACH(ga) ga[i] += std::cos(va[i]); }
};
Declare1(Sin);

inline tensor::type Sum(const tensor a, tensor::type s = 0) {
    EACH(a) s += a[i];
    return s;
}

inline tensor::type Dot(const tensor a, const tensor b, tensor::type s = 0) {
    DCheck(a.size() == b.size(), "arguments");
    EACH(a) s += a[i] * b[i];
    return s;
}

Diff operator+(Diff a, Diff b);
Diff operator+(tensor::type a, Diff b);
Diff operator+(Diff a, tensor::type b);

Diff operator-(Diff a, Diff b);
Diff operator-(Diff a);
Diff operator-(tensor::type a, Diff b);
Diff operator-(Diff a, tensor::type b);

Diff operator*(Diff a, Diff b);
Diff operator*(tensor::type a, Diff b);
Diff operator*(Diff a, tensor::type b);

Diff Inv(Diff a);
Diff operator/(Diff a, Diff b);
Diff operator/(tensor::type a, Diff b);
Diff operator/(Diff a, tensor::type b);

inline Diff& operator+=(Diff& a, Diff b) { return a = a + b; }
inline Diff& operator-=(Diff& a, Diff b) { return a = a - b; }
inline Diff& operator*=(Diff& a, Diff b) { return a = a * b; }
inline Diff& operator/=(Diff& a, Diff b) { return a = a / b; }

inline Diff& operator+=(Diff& a, tensor::type b) { return a = a + b; }
inline Diff& operator-=(Diff& a, tensor::type b) { return a = a - b; }
inline Diff& operator*=(Diff& a, tensor::type b) { return a = a * b; }
inline Diff& operator/=(Diff& a, tensor::type b) { return a = a / b; }

#define RELATION(NAME, OP)                                                                            \
    struct NAME : public Diff_vv {                                                                    \
        NAME(Diff a, Diff b) : Diff_vv(a, b) {}                                                     \
        void Forward(bool) override { EACH(v) v[i] = (va[i] OP vb[i]) ? 1 : 0; }                      \
    };                                                                                                \
    inline Diff operator OP(Diff a, Diff b) { return std::make_shared<NAME>(a, b); }                    \
    inline auto operator OP(tensor::type a, Diff b) { return Broadcast(Const(a), b->shape()) OP b; } \
    inline auto operator OP(Diff a, tensor::type b) { return a OP Broadcast(Const(b), a->shape()); }

RELATION(Greater, >);
RELATION(Less_, <);
RELATION(GreaterOrEqual, >=);
RELATION(LessOrEqual, <=);

#undef RELATION

Diff Min(Diff a, Diff b);
inline auto Min(tensor::type a, Diff b) { return Min(Broadcast(Const(a), b->shape()), b); }
inline auto Min(Diff a, tensor::type b) { return Min(a, Broadcast(Const(b), a->shape())); }

Diff Max(Diff a, Diff b);
inline auto Max(tensor::type a, Diff b) { return Max(Broadcast(Const(a), b->shape()), b); }
inline auto Max(Diff a, tensor::type b) { return Max(a, Broadcast(Const(b), a->shape())); }

Diff Concat(Diff a, Diff b);
Diff Reshape(Diff a, dim4 shape);
Diff VecMatMul(Diff a, Diff b);

Diff Sum(Diff a);
Diff Mean(Diff a);
Diff Stdev(Diff a, Diff mean_a, tensor::type k = 0);
inline Diff Stdev(Diff a) { return Stdev(a, Mean(a)); }

enum class ConvType { Same, Valid };
Diff Conv2D(Diff a, ConvType type, Diff b);
inline Diff Conv2D(Diff a, ConvType type, dim4 b_shape, std::shared_ptr<Init> init) { return Conv2D(a, type, Param(b_shape, init)); }

// TODO Unary horizontal Min and Max!

Diff RunningAverage(Diff a, float k);
Diff EpochMean(Diff a, float init);
Diff MeanSquareError(Diff a, Diff b);

Diff ValueCmp(Diff a, Diff b);
std::pair<Diff, Diff> GradCmp(Diff a);

Diff BinaryCrossEntropy(Diff ref, Diff out);
Diff BinaryAccuracy(Diff ref, Diff out);

inline Diff Softmax(Diff a) {
    Diff e = Exp(a);
    return e / Sum(e) << "softmax";
}

Diff BatchNorm(Diff a, tensor::type k);

inline Diff Bias(Diff a) {
    const dim4 s = a->batched() ? a->shape().pop_front() : a->shape();
    return a + Param(s) << "bias";
}

inline Diff Affine(Diff a, uint channels, std::shared_ptr<Init> init) {
    auto w = Param({channels, a->shape().back()}, init);
    return VecMatMul(a, w) << "affine";
}

inline Diff FullyConnected(Diff a, uint channels, std::shared_ptr<Init> init) {
    return Bias(Affine(a, channels, init));
}

float ComputePolynomial(float x, cspan<float> poly);
float ComputePolynomialDeriv(float x, cspan<float> poly);

Diff Kronecker(Diff a);
Diff Polynomial(Diff a, uint degree, uint channels, std::shared_ptr<Init> init);
