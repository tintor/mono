#include "core/optimizer.h"
#include "core/numeric.h"
#include "core/timestamp.h"

void Optimizer::Optimize(span<ParamT *> params) {
    for (auto p : params) {
        Timestamp ts;
        EACH(p->v) p->v[i] -= alpha * p->g[i];
        p->backward_ticks += ts.elapsed();
    }
}

void Momentum::Optimize(span<ParamT *> params) {
    if (ags.size() != params.size()) {
        ags.resize(params.size());
        FOR(i, params.size()) ags[i].reshape(params[i]->shape());
    }

    momentum_exp *= momentum;
    float alpha_with_correction = alpha / (1 - momentum_exp);

    FOR(j, params.size()) {
        Timestamp ts;
        vtensor &v = params[j]->v, &g = params[j]->g, &ag = ags[j];
        EACH(v) {
            ag[i] = momentum * ag[i] + (1 - momentum) * g[i];
            v[i] -= alpha_with_correction * ag[i];
        }
        params[j]->backward_ticks += ts.elapsed();
    }
}

template <bool Correct>
void RMSPropUpdate(ParamT *p, tensor agg, float alpha, float rmsprop, float rmsprop_correction, float rmsprop_eps) {
    Timestamp ts;
    vtensor &v = p->v, &g = p->g;
    EACH(v) {
        agg[i] = rmsprop * agg[i] + (1 - rmsprop) * sqr(g[i]);
        auto a = Correct ? (agg[i] * rmsprop_correction) : agg[i];
        v[i] -= alpha * g[i] / sqrt(a + rmsprop_eps);
    }
    p->backward_ticks += ts.elapsed();
}

void RMSProp::Optimize(span<ParamT *> params) {
    if (aggs.size() != params.size()) {
        aggs.resize(params.size());
        FOR(i, params.size()) aggs[i].reshape(params[i]->shape());
    }

    rmsprop_exp *= rmsprop;
    float rmsprop_correction = 1 / (1 - rmsprop_exp);

    if (rmsprop_correction >= 0.999999f) {
        FOR(j, params.size()) {
            RMSPropUpdate<false>(params[j], aggs[j], alpha, rmsprop, rmsprop_correction, rmsprop_eps);
        }
    } else {
        FOR(j, params.size()) {
            RMSPropUpdate<false>(params[j], aggs[j], alpha, rmsprop, 1, rmsprop_eps);
        }
    }
}

template <bool Correct>
void AdamUpdate(ParamT *p, tensor ag, tensor agg, float alpha_with_correction, float momentum, float rmsprop,
                float rmsprop_correction, float rmsprop_eps) {
    Timestamp ts;
    vtensor &v = p->v, &g = p->g;
    EACH(v) {
        ag[i] = momentum * ag[i] + (1 - momentum) * g[i];
        agg[i] = rmsprop * agg[i] + (1 - rmsprop) * sqr(g[i]);
        auto a = Correct ? (agg[i] * rmsprop_correction) : agg[i];
        v[i] -= alpha_with_correction * ag[i] / sqrt(a + rmsprop_eps);
    }
    p->backward_ticks += ts.elapsed();
}

void Adam::Optimize(span<ParamT *> params) {
    if (ags.size() != params.size()) {
        ags.resize(params.size());
        aggs.resize(params.size());
        FOR(i, params.size()) ags[i].reshape(params[i]->shape());
        FOR(i, params.size()) aggs[i].reshape(params[i]->shape());
    }

    momentum_exp *= momentum;
    float alpha_with_correction = alpha / (1 - momentum_exp);

    rmsprop_exp *= rmsprop;
    float rmsprop_correction = 1 / (1 - rmsprop_exp);

    if (rmsprop_correction >= 0.999999f) {
        FOR(j, params.size()) {
            AdamUpdate<true>(params[j], ags[j], aggs[j], alpha_with_correction, momentum, rmsprop, rmsprop_correction,
                             rmsprop_eps);
        }
    } else {
        FOR(j, params.size()) {
            AdamUpdate<false>(params[j], ags[j], aggs[j], alpha_with_correction, momentum, rmsprop, 1, rmsprop_eps);
        }
    }
}
