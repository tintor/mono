#include <unordered_set>
#include <map>
#include <iostream>

#include "core/model.h"
#include "core/vector.h"
#include "core/timestamp.h"

static void DFS(Diff a, std::unordered_set<DiffT*>& visited, std::vector<Diff>& out) {
    if (a == nullptr || visited.count(a.get())) return;
    visited.insert(a.get());
    for (Diff b : a->Inputs())
        if (b) DFS(b, visited, out);
    out.push_back(a);
}

std::vector<Diff> TopoSort(const cspan<Diff> heads) {
    std::vector<Diff> out;
    std::unordered_set<DiffT*> visited;
    for (Diff a : heads) DFS(a, visited, out);
    return out;
}

void Model::BeginEpoch(bool training) {
    for (auto p : m_begin_epoch_nodes) p->BeginEpoch(training);
}

void Model::Forward(bool training) {
    if (training) {
        for (auto p : m_forward_nodes) {
            Timestamp ts;
            p->Forward(true);
            p->forward_ticks += ts.elapsed();
        }
        return;
    }

    for (auto p : m_forward_nodes) if (p->inference) {
        Timestamp ts;
        p->Forward(false);
        p->forward_ticks += ts.elapsed();
    }
}

void Model::Backward(Diff loss) {
    for (auto p : m_params) {
        Timestamp ts;
        EACH(p->v) p->g[i] = 0;
        p->backward_ticks += ts.elapsed();
    }
    for (auto p : m_backward_nodes) {
        Timestamp ts;
        EACH(p->v) p->g[i] = 0;
        p->backward_ticks += ts.elapsed();
    }

    loss->g[0] = 1;

    for (auto p : m_backward_nodes) {
        Timestamp ts;
        p->Backward();
        p->backward_ticks += ts.elapsed();
    }

    optimizer->Optimize(span<ParamT*>(m_params));
}

void Model::EndEpoch(bool training) {
    for (auto p : m_end_epoch_nodes) p->EndEpoch(training);
}

bool Bounded(cspan<Diff> nodes, const tensor::type limit) {
    for (Diff p : nodes) {
        for (auto e : p->v)
            if (!std::isfinite(e) || abs(e) > limit) return false;
        for (auto e : p->g)
            if (!std::isfinite(e) || abs(e) > limit) return false;
    }
    return true;
}

auto ComputeIds(cspan<Diff> nodes) {
    std::map<DiffT*, std::string> ids;
    for (Diff p : nodes) {
        if (IsConst(p) && p->v.elements() == 1 && p->name.empty()) {
            ids.emplace(p.get(), fmt::format("{}", p->v[0]));
            continue;
        }

        std::string id = p->name.empty() ? "#0" : p->name;
        if (contains_value(ids, id)) {
            if (id == "#0") id = "#";
            int c = 1;
            while (contains_value(ids, fmt::format("{}{}", id, c))) c++;
            id = fmt::format("{}{}", id, c);
        }
        ids.emplace(p.get(), id);
    }
    return ids;
}

std::string Summary(const tensor v) {
    Aggregates<tensor::type> a(v.data(), v.data() + v.elements());
    return fmt::format("({} {} {}) {}", a.min, a.mean, a.max, sqrt(a.variance));
}

// TODO show number of gradients in g with very tiny absolute values
void Model::Print() const {
    auto ids = ComputeIds(m_nodes);

    ulong ftotal = 0, btotal = 0;
    for (Diff p : m_nodes)
        if (!(IsConst(p) && p->v.elements() == 1 && p->name.empty())) {
            ftotal += p->forward_ticks;
            btotal += p->backward_ticks;
        }
    const ulong total = ftotal + btotal;

    std::vector<std::string> table = {"id|type|inputs|shape|forward|backward|ratio|values|gradients|"};
    std::string os;
    for (Diff p : m_nodes)
        if (!(IsConst(p) && p->v.elements() == 1 && p->name.empty())) {
            os.clear();

            // id
            os += fmt::format("{}|", ids.at(p.get()));

            // type
            std::string type = TypeName(*p.get());
            if (IsConst(p)) type = "Const";
            if (type.back() == 'T') type.pop_back();
            os += fmt::format("{}|", type);

            // inputs
            for (Diff e : p->Inputs())
                if (e) os += fmt::format("{} ", ids.at(e.get()));
            os += '|';

            // shape
            os += fmt::format("{}|", p->shape());

            // forward
            os += fmt::format("{}|", (p->forward_ticks + 500) / 1000);

            // backward
            os += fmt::format("{}|", (p->backward_ticks + 500) / 1000);

            // ratio
            ulong ticks = p->forward_ticks + p->backward_ticks;
            os += fmt::format("{:.3f}|", 100.0f * ticks / total);

            // values
            size_t summary = 9;
            os += fmt::format("{}|", (p->v.elements() > summary) ? Summary(p->v) : std::string(p->v));

            // gradients
            os += fmt::format("{}|", (p->g.elements() > summary) ? Summary(p->g) : std::string(p->g));

            table << os;
        }

    os.clear();

    // id
    os += '|';

    // type
    os += '|';

    // inputs
    os += '|';

    // shape
    os += '|';

    // forward
    os += fmt::format("{}|", (ftotal + 500) / 1000);

    // backward
    os += fmt::format("{}|", (btotal + 500) / 1000);

    // ratio
    os += fmt::format("{:.3f}|", 100.0f);

    // values
    os += '|';

    // gradients
    os += '|';

    table << os;

    std::unique_lock lock(g_cout_mutex);
    PrintTable(table, '|', " ", {4, 5, 6});
}

void Model::Iterate(size_t iterations, Diff loss) {
    FOR(i, iterations) {
        BeginEpoch(true);
        Forward(true);
        Backward(loss);
        EndEpoch(true);
    }
}

bool HasBackward(Diff p) {
    DiffT::has_overload = true;
    p->Backward();
    return DiffT::has_overload;
}

Model::Model(std::initializer_list<Diff> heads) : m_nodes(TopoSort(heads)) {
    // TODO optimize it here:
    // - remove redundant diffs
    // - replace more expensive diffs with cheaper diffs
    // - fuse diffs
    // - if fanout of diff is 1 then its downstream diff can use = instead of += for gradients
    // - don't compute gradients which are not used!
    // - cpu std::vectorized kernels
    // - gpu std::vectorized kernels
    // - cpu multi-core kernels

    for (Diff p : m_nodes) {
        DiffT::has_overload = true;
        p->Forward(false);
        if (DiffT::has_overload) m_forward_nodes.push_back(p.get());
    }

    for (Diff p : m_nodes) {
        if (HasBackward(p)) m_backward_nodes.push_back(p.get());
    }
    std::reverse(m_backward_nodes.begin(), m_backward_nodes.end());

    for (Diff p : m_nodes) {
        DiffT::has_overload = true;
        p->BeginEpoch(false);
        if (DiffT::has_overload) m_begin_epoch_nodes.push_back(p.get());
    }

    for (Diff p : m_nodes) {
        DiffT::has_overload = true;
        p->EndEpoch(false);
        if (DiffT::has_overload) m_end_epoch_nodes.push_back(p.get());
    }

    for (Diff p : m_nodes) {
        auto param = dynamic_cast<ParamT*>(p.get());
        if (param) m_params.push_back(param);
    }
}

Metrics Model::Epoch(Diff loss, Diff accuracy, cspan<std::pair<Diff, tensor>> data,
        std::mt19937_64& random, bool verbose, uint epoch, bool backward) {
    Check(data.size() > 0);
    const dim_t B = data[0].first->dim(0);
    const dim_t N = data[0].second.dim(0);
    Check(N % B == 0, fmt::format("N % B must be 0. N:{} B:{}", N, B));

    for (const auto& [key, value] : data) {
        Check(key->dim(0) == B);
        Check(value.dim(0) == N);
        Check(value.shape().pop_front() == key->shape().pop_front(), fmt::format("{} vs {}", value.shape(), key->shape()));
    }

    if (m_samples.size() != N) {
        m_samples.resize(N);
        FOR(i, N) m_samples[i] = i;
    }
    std::shuffle(m_samples.begin(), m_samples.end(), random);

    std::string msg;
    Accumulator<float> a_loss, a_accuracy;
    Accumulator<float> a_f_ticks, a_b_ticks;
    ulong f_ticks = 0, b_ticks = 0;
    ulong message_ticks = 0;

    for (DiffT* p : m_begin_epoch_nodes) p->BeginEpoch(true);

    for (size_t i = 0; i < N; i += B) {
        for (const auto& [key, value] : data) {
            FOR(j, B) key->v.slice(j).copy_from(value.slice(m_samples[i + j]));
        }

        f_ticks += Duration([&]() { Forward(true); });
        if (backward) b_ticks += Duration([&]() { Backward(loss); });
        a_f_ticks << f_ticks;
        a_b_ticks << b_ticks;

        a_loss << loss->v[0];
        if (accuracy != nullptr) a_accuracy << accuracy->v[0];

        ulong ticks = f_ticks + b_ticks;
        if (ticks - message_ticks > long(1e10) && verbose) {
            message_ticks = ticks;

            for (char& c : msg) c = '\r';
            std::cout << msg;

            msg = fmt::format("{}: {}/{}", epoch, i + B, N);
            msg += fmt::format(" loss:{:.5f}", a_loss.mean());
            if (accuracy != nullptr) msg += fmt::format(" accuracy:{:.5f}", a_accuracy.mean());
            msg += "   ";
            std::cout << msg;
            std::cout.flush();
        }
    }

    for (DiffT* p : m_end_epoch_nodes) p->EndEpoch(true);

    if (verbose) {
        for (char& c : msg) c = '\r';
        std::cout << msg;
        msg.clear();

        fmt::print("{}: {}/{}", epoch, N, N);
        fmt::print(" loss:{.5f}", a_loss.mean());
        if (accuracy) fmt::print(" accuracy:{.5f}", a_accuracy.mean());
        fmt::print(" f_ticks:{}", ulong(a_f_ticks.mean()));
        fmt::print(" b_ticks:{}", ulong(a_b_ticks.mean()));
        fmt::print("\n");
    }

    Metrics metrics = {{"loss", a_loss.mean()}};
    if (accuracy) metrics.emplace("accuracy", a_accuracy.mean());
    return metrics;
}

void NormalizeDataset(tensor a) {
    vtensor q(a.shape().pop_front());

    const auto N = a.shape()[0];
    FOR(s, N) {
        auto sample = a.slice(s);
        EACH(q) q[i] += sample[i];
    }
    EACH(q) q[i] /= N;
    FOR(s, N) {
        auto sample = a.slice(s);
        EACH(q) sample[i] -= q[i];
    }

    EACH(q) q[i] = 0;
    FOR(s, N) {
        auto sample = a.slice(s);
        EACH(q) q[i] += sqr(sample[i]);
    }
    EACH(q) q[i] = 1 / sqrt(q[i] / N + 1e-8);
    FOR(s, N) {
        auto sample = a.slice(s);
        EACH(q) sample[i] *= q[i];
    }
}
