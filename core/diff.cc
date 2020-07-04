#include "core/diff.h"

thread_local bool DiffT::has_overload;

bool IsBroadcastable(dim4 a, dim4 b) { return a.ndims() != 0 && (a.elements() == 1 /*|| a == b.last(a.size)*/); }

struct BroadcastS : public Diff1 {
    BroadcastS(Diff a, dim4 b) : Diff1(a) { Reshape(b); }
    void Forward(bool) override { EACH(v) v[i] = va[0]; }
    void Backward() override {
        if (ga) EACH(g) ga[0] += g[i];
    }
};

/*struct BroadcastT : public Diff1 {
    BroadcastT(Diff a, dim4 b) : Diff1(a) { Reshape(b); }
    void Forward() override { EACH(v) v[i] = va[i % va.size]; }
    void Backward() override {
        if (ga) EACH(g) ga[i % ga.size] += g[i];
    }
};*/

Diff Broadcast(Diff a, dim4 b) {
    if (a->shape() == b) return a;
    if (a->elements() == 1) return make_shared<BroadcastS>(a, b);
    // if (a->shape == b.last(a->rank)) return make_shared<BroadcastT>(a, b);
    Check(false);
    return nullptr;
}

// ----------------------

struct Add_vv : public Diff_vv {
    Add_vv(Diff a, Diff b) : Diff_vv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] + vb[i]; }
    void Backward() override {
        EACH(ga) ga[i] += g[i];
        EACH(gb) gb[i] += g[i];
    }
};

struct Add_vs : public Diff_vs {
    Add_vs(Diff a, Diff b) : Diff_vs(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] + vb[0]; }
    void Backward() override {
        EACH(ga) ga[i] += g[i];
        if (gb) gb[0] += Sum(g, gb[0]);
    }
};

struct Add_mv : public Diff2 {
    Add_mv(Diff a, Diff b) : Diff2(a, b) {
        Check(a->shape().pop_front() == b->shape(),
            format("a:%s b:%s a.pop_front:%s", string(a->shape()), string(b->shape()), string(a->shape().pop_front())));
        Reshape(a->shape());
    }
    void Forward(bool) override {
        for (auto i : range<size_t>(0, va.elements(), vb.elements())) {
            for (auto j : range<size_t>(vb.elements())) v[i + j] = va[i + j] + vb[j];
        }
    }
    void Backward() override {
        EACH(ga) ga[i] += g[i];
        const auto m = va.shape()[0];
        EACH(gb) gb[i] += g[i] * m;
    }
};

Diff operator+(Diff a, Diff b) {
    dim4 as = a->shape(), bs = b->shape();
    if (a->elements() == 1 && !a->batched()) return make_shared<Add_vs>(b, a);
    if (b->elements() == 1 && !b->batched()) return make_shared<Add_vs>(a, b);
    if (a->shape() == b->shape()) return make_shared<Add_vv>(a, b);
    if (a->ndims() > b->ndims()) return make_shared<Add_mv>(a, b);
    if (b->ndims() > a->ndims()) return make_shared<Add_mv>(b, a);
    Fail(format("Incompatible shapes: %s vs %s", string(a->shape()), string(b->shape())));
    return nullptr;
}

Diff operator+(tensor::type a, Diff b) { return (a == 0) ? b : (b + Const(a)); }
Diff operator+(Diff a, tensor::type b) { return b + a; }

// ----------------------

struct Sub_vv : public Diff_vv {
    Sub_vv(Diff a, Diff b) : Diff_vv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] - vb[i]; }
    void Backward() override {
        EACH(ga) ga[i] += g[i];
        EACH(gb) gb[i] -= g[i];
    }
};

struct Sub_vs : public Diff_vs {
    Sub_vs(Diff a, Diff b) : Diff_vs(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] - vb[0]; }
    void Backward() override {
        EACH(ga) ga[i] += g[i];
        if (gb) gb[0] -= Sum(g, gb[0]);
    }
};

struct Sub_sv : public Diff_sv {
    Sub_sv(Diff a, Diff b) : Diff_sv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[0] - vb[i]; }
    void Backward() override {
        if (ga) ga[0] += Sum(g, ga[0]);
        EACH(gb) gb[i] -= g[i];
    }
};

struct Neg : public DiffA {
    Neg(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = -va[i]; }
    void Backward() override { EACH(ga) ga[i] -= g[i]; }
};

Diff operator-(Diff a, Diff b) {
    if (a->elements() == 1 && !a->batched()) return make_shared<Sub_sv>(a, b);
    if (b->elements() == 1 && !b->batched()) return make_shared<Sub_vs>(a, b);
    return make_shared<Sub_vv>(a, b);
}

Diff operator-(Diff a) { return make_shared<Neg>(a); }
Diff operator-(tensor::type a, Diff b) { return (a == 0) ? -b : (Const(a) - b); }
Diff operator-(Diff a, tensor::type b) { return (b == 0) ? a : (a - Const(b)); }

// ----------------------

struct Mul_vs : public Diff_vs {
    Mul_vs(Diff a, Diff b) : Diff_vs(a, b) {
        Check(b->elements() == 1 && !b->batched());
        Reshape(a->shape());
    }
    void Forward(bool) override { EACH(v) v[i] = va[i] * vb[0]; }
    void Backward() override {
        EACH(ga) ga[i] += g[i] * vb[0];
        if (gb) gb[0] += Dot(g, va);
    }
};

struct Mul_vv : public Diff_vv {
    Mul_vv(Diff a, Diff b) : Diff_vv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] * vb[i]; }
    void Backward() override {
        EACH(ga) ga[i] += g[i] * vb[i];
        EACH(gb) gb[i] += g[i] * va[i];
    }
};

struct Mul_mv : public Diff2 {
    Mul_mv(Diff a, Diff b) : Diff2(a, b) {
        Check(a->shape().pop_front() == b->shape(),
            format("a:%s b:%s a.pop_front:%s", string(a->shape()), string(b->shape()), string(a->shape().pop_front())));
        Reshape(a->shape());
    }
    void Forward(bool) override {
        for (auto j : range<size_t>(0, va.elements(), vb.elements())) {
            EACH(vb) v[j + i] = va[j + i] * vb[i];
        }
    }
    void Backward() override {
        if (ga && gb) {
            for (auto j : range<size_t>(0, va.elements(), vb.elements())) {
                EACH(vb) {
                    ga[j + i] += g[j + i] * vb[i];
                    gb[i] += g[j + i] * va[j + i];
                }
            }
        } else if (ga) {
            for (auto j : range<size_t>(0, va.elements(), vb.elements())) {
                EACH(vb) ga[j + i] += g[j + i] * vb[i];
            }
        } else if (gb) {
            for (auto j : range<size_t>(0, va.elements(), vb.elements())) {
                EACH(vb) gb[i] += g[j + i] * va[j + i];
            }
        }
    }
};

Diff operator*(Diff a, Diff b) {
    if (a->elements() == 1 && !a->batched()) return make_shared<Mul_vs>(b, a);
    if (b->elements() == 1 && !b->batched()) return make_shared<Mul_vs>(a, b);
    if (a->shape() == b->shape()) return make_shared<Mul_vv>(a, b);
    if (a->ndims() > b->ndims()) return make_shared<Mul_mv>(a, b);
    if (b->ndims() > a->ndims()) return make_shared<Mul_mv>(b, a);
    Fail(format("Incompatible shapes: %s vs %s", string(a->shape()), string(b->shape())));
    return nullptr;
}

Diff operator*(tensor::type a, Diff b) {
    if (a == 1) return b;
    if (a == 0) return Const(b->shape(), 0);
    return b * Const(a);
}

Diff operator*(Diff a, tensor::type b) {
    if (b == 1) return a;
    if (b == 0) return Const(a->shape(), 0);
    return a * Const(b);
}

// ----------------------

struct Div_vv : public Diff_vv {
    Div_vv(Diff a, Diff b) : Diff_vv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] / vb[i]; }
    void Backward() override {
        EACH(ga) ga[i] += g[i] / vb[i];
        EACH(gb) gb[i] -= g[i] * v[i] / vb[i];
    }
};

struct Div_vs : public Diff_vs {
    Div_vs(Diff a, Diff b) : Diff_vs(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[i] / vb[0]; }
    void Backward() override {
        EACH(ga) ga[i] += g[i] / vb[0];
        if (gb) EACH(g) gb[0] -= g[i] * v[i] / vb[0];
    }
};

struct Div_sv : public Diff_sv {
    Div_sv(Diff a, Diff b) : Diff_sv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = va[0] / vb[i]; }
    void Backward() override {
        if (ga) EACH(g) ga[0] += g[i] / vb[i];
        EACH(gb) gb[i] -= g[i] * v[0] / sqr(vb[i]);
    }
};

struct InvT : public DiffA {
    InvT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = 1 / va[i]; }
    void Backward() override { EACH(ga) ga[i] -= g[i] * sqr(v[i]); }
};

Diff Inv(Diff a) { return make_shared<InvT>(a); }

Diff operator/(Diff a, Diff b) {
    if (a->elements() == 1 && !a->batched()) return make_shared<Div_sv>(a, b);
    if (b->elements() == 1 && !b->batched()) return make_shared<Mul_vs>(a, Inv(b));
    return make_shared<Div_vv>(a, b);
}

Diff operator/(tensor::type a, Diff b) { return (a == 1) ? Inv(b) : (Const(a) / b); }
Diff operator/(Diff a, tensor::type b) { return a * Const(1 / b); }

// ----------------------

struct PowT : public Diff2 {
    PowT(Diff a, Diff b) : Diff2(a, b) {
        Check(a->shape() == b->shape() || (a->elements() == 1 && !a->batched()) || (b->elements() == 1 && !b->batched()));
        Reshape((a->elements() > b->elements()) ? a->shape() : b->shape());
    }
    void Forward(bool) override {
        if (a->elements() == 1) {
            if (b->elements() == 1) {
                EACH(v) v[i] = std::pow(va[0], vb[0]);
            } else {
                EACH(v) v[i] = std::pow(va[0], vb[i]);
            }
        } else {
            if (b->elements() == 1) {
                EACH(v) v[i] = std::pow(va[i], vb[0]);
            } else {
                EACH(v) v[i] = std::pow(va[i], vb[i]);
            }
        }
    }
    void Backward() override {
        if (a->elements() == 1) {
            if (b->elements() == 1) {
                if (ga) ga[0] += g[0] * vb[0] * v[0] / va[0];
                if (gb) gb[0] += g[0] * v[0] * std::log(va[0]);
            } else {
                if (ga) EACH(g) ga[0] += g[i] * vb[i] * v[i] / va[0];
                if (gb) EACH(g) gb[i] += g[i] * v[i] * std::log(va[0]);
            }
        } else {
            if (b->elements() == 1) {
                if (ga) EACH(g) ga[i] += g[i] * vb[0] * v[i] / va[i];
                if (gb) EACH(g) gb[0] += g[i] * v[i] * std::log(va[i]);
            } else {
                if (ga) EACH(g) ga[i] += g[i] * vb[i] * v[i] / va[i];
                if (gb) EACH(g) gb[i] += g[i] * v[i] * std::log(va[i]);
            }
        }
    }
};

Diff Pow(Diff a, Diff b) { return make_shared<PowT>(a, b); }

// ----------------------

struct MinT : public Diff_vv {
    MinT(Diff a, Diff b) : Diff_vv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = min(va[i], vb[i]); }
    void Backward() override {
        EACH(ga) ga[i] += (va[i] < vb[i]) * g[i];
        EACH(gb) gb[i] += (vb[i] < va[i]) * g[i];
    }
};

Diff Min(Diff a, Diff b) { return make_shared<MinT>(a, b); }

struct MaxT : public Diff_vv {
    MaxT(Diff a, Diff b) : Diff_vv(a, b) {}
    void Forward(bool) override { EACH(v) v[i] = max(va[i], vb[i]); }
    void Backward() override {
        EACH(ga) ga[i] += (va[i] > vb[i]) * g[i];
        EACH(gb) gb[i] += (vb[i] > va[i]) * g[i];
    }
};

Diff Max(Diff a, Diff b) { return make_shared<MaxT>(a, b); }

// ----------------------

// TODO Concat more than two inputs
// TODO Concat along given dimension (ie. channel dim)
struct ConcatT : public Diff2 {
    ConcatT(Diff a, Diff b) : Diff2(a, b) {
        // TODO generalize for more dimensions
        Check(a->ndims() == 1);
        Check(b->ndims() == 1);
        Reshape({uint(a->elements() + b->elements())});
    }

    void Forward(bool) override {
        EACH(v) v[i] = va[i];
        EACH(v) v[i + va.elements()] = vb[i];
    }

    void Backward() override {
        EACH(ga) ga[i] += g[i];
        EACH(gb) gb[i] += g[i + va.elements()];
    }
};

Diff Concat(Diff a, Diff b) { return make_shared<ConcatT>(a, b); }

// ----------------------

struct ReshapeT : public Diff1 {
    ReshapeT(Diff a, dim4 shape) : Diff1(a) {
        if (a->batched()) shape = shape.push_front(a->dim(0), a->shape().name(0));
        Check(a->elements() == shape.elements());
        v.reshape(shape);
        if (a->g) g.reshape(shape);
    }

    void Forward(bool) override { EACH(v) v[i] = va[i]; }
    void Backward() override { EACH(ga) ga[i] = g[i]; }
};

Diff Reshape(Diff a, dim4 shape) { return make_shared<ReshapeT>(a, shape); }

// ----------------------

// m can be zero, one or more dimensions
// p and q are exactly one dimension
// v{m,p} x m{q,p} -> {m,q}
struct VecMatMulT : public Diff2 {
    VecMatMulT(Diff a, Diff b) : Diff2(a, b) {
        dim4 as = a->shape(), bs = b->shape();
        Check(as.ndims() > 0);
        Check(bs.ndims() == 2, format("a:%s b:%s", as, bs));
        Check(as.back() == bs.back());
        Reshape(as.pop_back().push_back(bs[0], bs.name(0)));
    }

    void Forward(bool) override {
        const size_t m = va.elements() / va.shape().back();
        const size_t q = vb.dim(0);
        const size_t p = vb.dim(1);

        for (auto im : range(m)) {
            for (auto iq : range(q)) {
                tensor::type s = 0;
                for (auto ip : range(p)) s += va[im * p + ip] * vb[iq * p + ip];
                v[im * q + iq] = s;
            }
        }
    }

    void Backward() override {
        const size_t m = va.elements() / va.shape().back();
        const size_t q = vb.dim(0);
        const size_t p = vb.dim(1);

        if (ga) {
            for (auto im : range(m))
                for (auto ip : range(p)) {
                    tensor::type s = ga[im * p + ip];
                    for (auto iq : range(q)) s += g[im * q + iq] * vb[iq * p + ip];
                    ga[im * p + ip] = s;
                }
        }
        if (gb) {
            for (auto iq : range(q))
                for (auto ip : range(p)) {
                    tensor::type s = gb[iq * p + ip];
                    for (auto im : range(m)) s += g[im * q + iq] * va[im * p + ip];
                    gb[iq * p + ip] = s;
                }
        }
    }
};

Diff VecMatMul(Diff a, Diff b) { return make_shared<VecMatMulT>(a, b); }

// ----------------------

struct SumT : public Diff1 {
    SumT(Diff a) : Diff1(a) { Reshape({}); }
    void Forward(bool) override { v[0] = Sum(va); }
    void Backward() override { EACH(ga) ga[i] += g[0]; }
};

Diff Sum(Diff a) { return make_shared<SumT>(a); }

struct MeanT : public Diff1 {
    MeanT(Diff a) : Diff1(a) { Reshape({}); }
    void Forward(bool) override { v[0] = Sum(va) / a->elements(); }
    void Backward() override {
        const tensor::type d = g[0] / a->elements();
        EACH(ga) ga[i] += d;
    }
};

Diff Mean(Diff a) { return make_shared<MeanT>(a); }

struct StdevT : public Diff2 {
    StdevT(Diff a, Diff b, tensor::type k) : Diff2(a, b), k(k) {
        Check(b->elements() == 1 && !b->batched());
        Reshape({});
    }
    void Forward(bool) override {
        double s = 0;
        EACH(v) s += sqr(double(va[i] - vb[0]));
        v[0] = std::sqrt(s / a->elements() + k);
    }
    void Backward() override {
        const tensor::type d = g[0] / (a->elements() * v[0]);
        EACH(ga) ga[i] += (va[i] - vb[0]) * d;
    }
    tensor::type k;
};

Diff Stdev(Diff a, Diff mean_a, tensor::type k) { return make_shared<StdevT>(a, mean_a, k); }

// ----------------------

struct Conv1D : public Diff2 {
    Conv1D(Diff a, Diff b, int offset) : Diff2(a, b), offset(offset) {
        Check(a->ndims() == 1);
        Check(b->ndims() == 1);
        Check(!b->batched());
        Reshape(a->shape());
    }

    void Forward(bool) override {
        EACH(v) {
            tensor::type sum = 0;
            // TODO check
            size_t begin = (offset > i) ? offset - i : 0;
            size_t end = min<size_t>(vb.elements(), offset - i + va.elements());
            for (size_t j = begin; j < end; j++) sum += va[i + j - offset] * vb[j];
            v[i] = sum;
        }
    }

    void Backward() override { Check(false, "not implemented"); }

    int offset;
};

struct Deconv1D : public Diff2 {
    Deconv1D(Diff a, Diff b) : Diff2(a, b) {}
};

// ----------------------

template<bool valid>
struct Conv2DT : public Diff2 {
    static dim_t Valid(dim_t a, dim_t b) {
        if (!valid) return a;
        Check(a >= b);
        return a - b + 1;
    }

    Conv2DT(Diff a, Diff b) : Diff2(a, b) {
        const dim_t w = Valid(a->dim(1), b->dim(1));
        const dim_t h = Valid(a->dim(2), b->dim(2));
        Reshape({a->dim(0), w, h, b->dim(0), 'b', 'w', 'h', 'c'});
    }
    void Forward(bool) override {
        const dim_t Batch = dim(0);
        const dim_t W = dim(1);
        const dim_t H = dim(2);
        const dim_t C = dim(3);

        const dim_t AW = a->dim(1);
        const dim_t AH = a->dim(2);
        const dim_t AC = a->dim(3);
        const dim_t BW = b->dim(1);
        const dim_t BH = b->dim(2);

        // TODO optimize order of for loops
        // TODO slice q and c to reduce amount of inner computation
        FOR(q, Batch) FOR(x, W) FOR(y, H) FOR(c, C) {
            tensor::type s = 0;
            // TODO unroll inner loop for common values of KW KH AC
            FOR(i, BW) FOR(j, BH) FOR(e, AC) {
                dim_t xx = valid ? x + i : (x - BW / 2 + i);
                dim_t yy = valid ? y + j : (y - BH / 2 + j);
                if (valid || (xx < AW && yy < AH)) s += va(q, xx, yy, e) * vb(c, i, j, e);
            }
            v(q, x, y, c) = s;
        }
    }
    void Backward() override {
        const dim_t Batch = dim(0);
        const dim_t W = dim(1);
        const dim_t H = dim(2);
        const dim_t C = dim(3);

        const dim_t AW = a->dim(1);
        const dim_t AH = a->dim(2);
        const dim_t AC = a->dim(3);
        const dim_t BW = b->dim(1);
        const dim_t BH = b->dim(2);

        if (ga) {
            FOR(q, Batch) FOR(x, W) FOR(y, H) FOR(c, C) {
                FOR(i, BW) FOR(j, BH) FOR(e, AC) {
                    dim_t xx = valid ? x + i : (x - BW / 2 + i);
                    dim_t yy = valid ? y + j : (y - BH / 2 + j);
                    if (valid || (xx < AW && yy < AH)) ga(q, xx, yy, e) += g(q, x, y, c) * vb(c, i, j, e);
                }
            }
        }
        if (gb) {
            FOR(q, Batch) FOR(x, W) FOR(y, H) FOR(c, C) {
                FOR(i, BW) FOR(j, BH) FOR(e, AC) {
                    dim_t xx = valid ? x + i : (x - BW / 2 + i);
                    dim_t yy = valid ? y + j : (y - BH / 2 + j);
                    if (valid || (xx < AW && yy < AH)) gb(c, i, j, e) += va(q, xx, yy, e) * g(q, x, y, c);
                }
            }
        }
    }
};

Diff Conv2D(Diff a, ConvType type, Diff b) {
    Check(a->shape().dims() == "bwhc", format("a: expected 'bwhc', but got '%s'", a->shape().dims()));
    Check(b->shape().dims() == "iwhc", format("b: expected 'iwhc', but got '%s'", b->shape().dims()));
    Check(a->dim(3) == b->dim(3));
    Check(b->dim(1) % 2 == 1);
    Check(b->dim(2) % 2 == 1);
    switch (type) {
        case ConvType::Same: return make_shared<Conv2DT<false>>(a, b);
        case ConvType::Valid: return make_shared<Conv2DT<true>>(a, b);
    }
}

// ------------------------

struct MaxPool1D : public Diff1 {
    MaxPool1D(Diff a) : Diff1(a) {
        Check(a->ndims() == 1);
        Check(!a->batched());
        const uint m = (a->dim(0) + 1) / 2;
        Reshape({m});
    }

    void Forward(bool) override {
        const size_t m = a->dim(0) / 2;
        for (size_t i = 0; i < m; i++) v[i] = max(va[i * 2], va[i * 2 + 1]);
        if (a->dim(0) % 2 == 1) v[m] = va[m * 2];
    }

    void Backward() override {
        if (!ga) return;
        Check(false);
        const size_t m = dim(0);
        for (size_t i = 0; i < m; i++) g[i] = max(va[i * 2], va[i * 2 + 1]);
    }
};

struct MaxPool2D : public Diff1 {
    MaxPool2D(Diff a);
    void Forward(bool) override;
    void Backward() override;
};

MaxPool2D::MaxPool2D(Diff a) : Diff1(a) {
    Check(a->ndims() == 2);
    const uint m = (a->dim(0) + 1) / 2;
    const uint n = (a->dim(1) + 1) / 2;
    Reshape({m, n});
}

void MaxPool2D::Forward(bool) {
    // TODO edge condition on last row / column
    Check(a->dim(0) % 2 == 0);
    Check(a->dim(1) % 2 == 0);

    const uint m = dim(0);
    const uint n = dim(1);
    for (uint i = 0; i < m; i++) {
        for (uint j = 0; j < n; j++) {
            const uint p = i * 2, q = j * 2;
            v(i, j) = max(va(p, q), va(p + 1, q), va(p, q + 1), va(p + 1, q + 1));
        }
    }
}

void MaxPool2D::Backward() {
    if (!ga) return;
    Check(false);
    const uint m = dim(0);
    const uint n = dim(1);
    for (uint i = 0; i < m; i++) {
        for (uint j = 0; j < n; j++) {
            const uint p = i * 2, q = j * 2;
            g(i, j) = max(va(p, q), va(p + 1, q), va(p, q + 1), va(p + 1, q + 1));
        }
    }
}

// ------------------------

struct RunningAverageT : public Diff1 {
    RunningAverageT(Diff a, float k) : Diff1(a), k(k) { v.reshape(a->shape()); }
    void Forward(bool) override { EACH(v) v[i] = v[i] * (1 - k) + va[i] * k; }
    tensor::type k;
};

Diff RunningAverage(Diff a, float k) { return make_shared<RunningAverageT>(a, k); }

// Returns mean of all input values from prev epoch OR 0 if first epoch.
struct EpochMeanT : public Diff1 {
    EpochMeanT(Diff a) : Diff1(a) { v.reshape({}); }
    void Forward(bool) override {
        EACH(va) sum += va[i];
        count += va.elements();
    }
    void EndEpoch(bool) override {
        v[0] = sum / count;
        sum = 0;
        count = 0;
    }

    double sum = 0;
    uint count = 0;
};

Diff EpochMean(Diff a, float init) {
    auto p = make_shared<EpochMeanT>(a);
    p->v[0] = init;
    return p;
}

struct MeanSquareErrorT : public Diff2 {
    MeanSquareErrorT(Diff a, Diff b) : Diff2(a, b) {
        Check(a->shape() == b->shape(), format("%s vs %s", string(a->shape()), string(b->shape())));
        Reshape({});
    }
    void Forward(bool) override {
        double s = 0;
        EACH(va) s += sqr(va[i] - vb[i]);
        v[0] = s / a->elements();
    }
    void Backward() override {
        const tensor::type d = (2.0 * g[0]) / a->elements();
        EACH(ga) ga[i] += (va[i] - vb[i]) * d;
        EACH(gb) gb[i] += (vb[i] - va[i]) * d;
    }
};

Diff MeanSquareError(Diff a, Diff b) { return make_shared<MeanSquareErrorT>(a, b); }

// ------------------------

// TODO move to util
template <typename T>
inline T RelativeError(T a, T b) {
    auto abs_error = abs(a - b);
    return abs_error / max(abs(a), abs(b));
}

template <typename T>
inline bool EqualFP(T a, T b) {
    auto abs_error = abs(a - b);
    if (a == 0 || b == 0) return abs_error <= 1e-6;
    return abs_error / max(abs(a), abs(b)) <= 1e-6;
}

template <typename T>
inline void CheckEqualFP(T a, T b) {
    if (!EqualFP(a, b))
        Fail(format("Expected equality of %.10f and %.10f (abs error %g, rel error %g)", a, b, abs(a - b),
                    RelativeError(a, b)));
}

struct ValueCmpT : public Diff_vv {
    ValueCmpT(Diff a, Diff b) : Diff_vv(a, b) {}
    void Forward(bool) override {
        EACH(v) {
            v[i] = va[i];
            CheckEqualFP(va[i], vb[i]);
        }
    }
    void Backward() override {
        EACH(ga) ga[i] = g[i];
        EACH(gb) gb[i] = g[i];
    }
};

Diff ValueCmp(Diff a, Diff b) { return make_shared<ValueCmpT>(a, b); }

struct GradCmpT : public DiffA {
    GradCmpT(Diff a) : DiffA(a) {}
    void Forward(bool) override { EACH(v) v[i] = va[i]; }
    void Backward() override {
        if (++counter != other->counter) return;
        EACH(ga) ga[i] = g[i];
        EACH(g) CheckEqualFP(g[i], other->g[i]);
    }
    void EndEpoch(bool) override {
        if (counter != other->counter) Fail(format("%s != %s", counter, other->counter));
    }

    uint counter = 0;
    GradCmpT* other;  // can't use shared_ptr because cyclic references
};

pair<Diff, Diff> GradCmp(Diff a) {
    auto p = make_shared<GradCmpT>(a);
    auto q = make_shared<GradCmpT>(a);
    p->other = q.get();
    q->other = p.get();
    return {p, q};
}

// ------------------------

// TODO - BinaryCrossEntropy(Logistic()) can be merged into one node, with cheaper backward and more numericaly stable
// TODO - same for CategoricalCrossEntropy(Softmax())

struct BinaryCrossEntropyT : public Diff2 {
    constexpr static tensor::type eps = 1e-6, one = 1;

    BinaryCrossEntropyT(Diff ref, Diff out) : Diff2(ref, out) {
        Check(a->batched() == b->batched());
        Check(a->shape().normalized() == b->shape().normalized(), format("%s vs %s", string(a->shape()), string(b->shape())));
        Check(out->g);
        Check(!ref->g);
        Reshape({});
    }
    void Forward(bool) override {
        double s = 0;
        EACH(va) {
            s -= va[i] * std::log(max(eps, vb[i]));
            s -= (one - va[i]) * std::log(max(eps, one - vb[i]));
        }
        v[0] = s / a->elements();
    }
    void Backward() override {
        Check(g.ndims() == 0 || g[0] == 1);
        EACH(gb) {
            // TODO try removing the max
            auto p = va[i] / max(eps, vb[i]) * (eps < vb[i]);
            auto q = (one - va[i]) / max(eps, one - vb[i]) * (eps < one - vb[i]);
            gb[i] -= (p - q) / a->elements();  // not multiplying with g[0] == 1
        }
    }
};

Diff BinaryCrossEntropy(Diff ref, Diff out) { return make_shared<BinaryCrossEntropyT>(ref, out); }

struct BinaryAccuracyT : public Diff2 {
    BinaryAccuracyT(Diff ref, Diff out) : Diff2(ref, out) {
        Check(a->batched() == b->batched());
        Check(a->shape().normalized() == b->shape().normalized(), format("%s vs %s", string(a->shape()), string(b->shape())));
        Check(!ref->g);
        v.reshape({});
    }
    void Forward(bool) override {
        double s = 0;
        EACH(va) s += std::abs(va[i] - vb[i]) < 0.5f;
        v[0] = s / a->elements();
    }
};

Diff BinaryAccuracy(Diff ref, Diff out) { return make_shared<BinaryAccuracyT>(ref, out); }

// ---------------------------

struct TrainEpochAverageT : public DiffA {
    TrainEpochAverageT(Diff a) : DiffA(a) { s.reshape(a->shape()); }
    void BeginEpoch(bool training) override {
        if (training) {
            EACH(s) s[i] = 0;
            count = 0;
            divided = false;
        } else {
            if (!divided) {
                EACH(s) s[i] /= count;
                divided = true;
            }
            EACH(s) v[i] = s[i];
        }
    }
    void Forward(bool training) override {
        if (training) {
            EACH(va) s[i] += va[i];
            EACH(va) v[i] = va[i];
            count += 1;
        }
    }
    void Backward() override {
        EACH(ga) ga[i] += g[i];
    }

    vtensor s;
    uint count;
    bool divided;
};

Declare1(TrainEpochAverage);

Diff BatchNorm(Diff a, tensor::type k) {
    auto mean = Mean(a) << "bn_mean";
    auto stdev = Stdev(a, mean, k) << "bn_stdev";

    mean->inference = false;
    stdev->inference = false;

    auto imean = TrainEpochAverage(mean) << "bn_imean";
    auto istdev = TrainEpochAverage(stdev) << "bn_istdev";
    auto norm = (a - imean) / istdev << "bn_norm";

    const dim4 s = a->batched() ? a->shape().pop_front() : a->shape();
    auto gamma = Param(s, 1) << "bn_gamma";
    auto beta = Param(s) << "bn_beta";

    return (norm * gamma + beta) << "bn";
}

struct KroneckerT : public Diff1 {
    KroneckerT(Diff a) : Diff1(a) {
        Check(a->ndims() == 2 && a->batched(), string(a->shape()));
        Reshape(a->shape().push_back(a->shape().back()));
    }
    void Forward(bool) override {
        const uint B = dim(0);
        const uint N = dim(1);
        FOR(k, B) FOR(i, N) FOR(j, N) v(k, i, j) = va(k, i) * va(k, j);
    }
    void Backward() override {
        const uint B = ga.dim(0);
        const uint N = dim(1);
        FOR(k, B) FOR(i, N) FOR(j, N) {
            ga(k, i) += g(k, i, j) * va(k, j);
            ga(k, j) += g(k, i, j) * va(k, i);
        }
    }
};

Diff Kronecker(Diff a) { return make_shared<KroneckerT>(a); }

float ComputePolynomial(float x, cspan<float> poly) {
    if (poly.size() == 0) return 0;
    tensor::type s = poly[0];
    tensor::type e = x;
    for (auto m = 1; m < poly.size(); m++) {
        s += e * poly[m];
        e *= x;
    }
    return s;
}

float ComputePolynomialDeriv(float x, cspan<float> poly) {
    if (poly.size() <= 1) return 0;
    tensor::type s = poly[1];
    tensor::type e = x;
    for (auto m = 2; m < poly.size(); m++) {
        s += e * m * poly[m];
        e *= x;
    }
    return s;
}

struct PolynomialT : public Diff2 {
    PolynomialT(Diff a, uint degree, uint channels, std::shared_ptr<Init> init) : Diff2(a, Param({channels, degree + 1}, init)) {
        Check(a->ndims() == 2 && a->batched(), string(a->shape()));
        Check(degree >= 1);
        Reshape(a->shape().push_back(channels, 'c'));
    }
    void Forward(bool) override {
        const uint B = dim(0);
        const uint N = dim(1);
        const uint C = dim(2);

        FOR(k, B)
            FOR(i, N)
                FOR(j, C)
                    v(k, i, j) = ComputePolynomial(va(k, i), b->v.slice(j));
    }
    void Backward() override {
        const uint B = dim(0);
        const uint N = dim(1);
        const uint C = dim(2);
        const uint P = b->dim(0);

        if (ga)
        FOR(k, B)
            FOR(i, N)
                FOR(j, C)
                    ga(k, i) += g(k, i, j) * ComputePolynomialDeriv(va(k, i), b->v.slice(j));

        if (gb)
        FOR(k, B)
            FOR(i, N)
                FOR(j, C)
                    FOR(m, P)
                        gb(j, m) += g(k, i, j) * pow(va(k, i), m);
    }
};

Diff Polynomial(Diff a, uint degree, uint channels, std::shared_ptr<Init> init) { return make_shared<PolynomialT>(a, degree, channels, init); }
