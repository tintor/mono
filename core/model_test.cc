#include "core/model.h"
#include "core/thread.h"
#include "core/range.h"
#include "core/vector.h"
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("diff: grad check Pow()", "[diff]") {
    FOR(c, 4) {
        auto a = Param((c & 1) ? dim4{3} : dim4{});
        auto b = Param((c & 2) ? dim4{3} : dim4{});
        auto [a0, a1] = GradCmp(a);
        auto [b0, b1] = GradCmp(b);
        auto p = ValueCmp(Exp(b0 * Log(a0)), Pow(a1, b1));
        Model model({p});

        std::mt19937_64 random(1);
        FOR(i, 100000) {
            std::uniform_real_distribution<double> dis(0.1, 3);
            EACH(a->v) a->v[i] = dis(random);
            EACH(b->v) b->v[i] = dis(random);
            model.Forward(true);
            model.Backward(p);
        }
    }
}

TEST_CASE("diff: minimize circle", "[diff]") {
    auto x = Param({}) << "x";
    auto y = Param({}) << "y";
    x->v[0] = 3.14f;
    y->v[0] = 2.16f;
    Minimize(Sqr(x) + Sqr(y), 0.1, 70);

    REQUIRE(abs(x->v[0]) < 1e-6);
    REQUIRE(abs(y->v[0]) < 1e-6);
}

void Test(std::string_view name, Model& model, Diff loss, Diff x, Diff ref, Diff b, std::vector<std::string>& table) {
    auto row = table.begin();
    if (name == "sgd") *row += "b|";
    *row++ += fmt::format("{}|", name);

    for (float e : range(-1.f, 1.01f, 0.1f)) {
        x->v[0] = e;
        ref->v[0] = 0;
        model.optimizer->Reset();
        model.Forward(true);
        if (name == "sgd") *row += fmt::format("{}|", b->v[0]);
        int count = 250;
        for (int i = 0; i < 250; i++) {
            model.Forward(true);
            model.Backward(loss);
            if (b->v[0] <= 0.05) {
                count = i + 1;
                break;
            }
        }
        REQUIRE(b->v[0] <= 0.05);
        *row++ += fmt::format("{}|", count);
    }

    for (float e : range(-1.f, 1.01f, 0.1f)) {
        x->v[0] = e;
        ref->v[0] = 1;
        model.optimizer->Reset();
        model.Forward(true);
        if (name == "sgd") *row += fmt::format("{}|", b->v[0]);
        int count = 250;
        for (int i = 0; i < 250; i++) {
            model.Forward(true);
            model.Backward(loss);
            if (b->v[0] >= 0.95) {
                count = i + 1;
                break;
            }
        }
        REQUIRE(b->v[0] >= 0.95);
        *row++ += fmt::format("{}|", count);
    }
}

#include <iostream>

TEST_CASE("diff: minimize binary cross entropy (grad check)", "[diff]") {
    auto x = Param({}) << "x";
    auto ref = Const(0);
    auto b = Logistic(x) << "b";

    // grad check BinaryCrossEntropy
    auto [b0, b1] = GradCmp(b);
    auto one = Mean(-(ref * Log(Max(1e-6, b0)) + (1 - ref) * Log(Max(1e-6, 1 - b0))));
    auto two = BinaryCrossEntropy(ref, b1);
    auto loss = ValueCmp(one, two);
    Model model({loss});

    std::vector<std::string> table;
    table << std::string("x|r|");
    for (float e : range(-1.f, 1.01f, 0.1f)) table << fmt::format("{:+.1f}|0|", e);
    for (float e : range(-1.f, 1.01f, 0.1f)) table << fmt::format("{:+.1f}|1|", e);

    Test("sgd", model, loss, x, ref, b, table);

    model.optimizer = std::make_shared<Momentum>();
    Test("momentum", model, loss, x, ref, b, table);

    model.optimizer = std::make_shared<RMSProp>();
    Test("rmsprop", model, loss, x, ref, b, table);

    model.optimizer = std::make_shared<Adam>();
    Test("adam", model, loss, x, ref, b, table);

    PrintTable(table, '|', " ", {3, 4, 5, 6});
}

TEST_CASE("diff: minimize booth", "[diff]") {
    std::mt19937_64 random(1);
    auto init = std::make_shared<NormalInit>(1, random);
    auto x = Param({}, init) << "x";
    auto y = Param({}, init) << "y";
    Minimize(Sqr(x + 2 * y - 7) + Sqr(2 * x + y - 5), 0.01, 1000);

    REQUIRE(abs(x->v[0] - 1) <= 2e-5);
    REQUIRE(abs(y->v[0] - 3) <= 2e-5);
}

TEST_CASE("diff: minimize rastrigin", "[diff]") {
    std::mt19937_64 random(1);
    auto init = std::make_shared<NormalInit>(1, random);
    auto x = Param({}, init) << "x";
    auto y = Param({}, init) << "y";
    Minimize(20 + (x * x - 10 * Cos(2 * PI * x)) + (y * y - 10 * Cos(2 * PI * y)), 0.01, 2000);

    // REQUIRE(abs(x->v[0]) < 1e-5);
    // REQUIRE(abs(y->v[0]) < 1e-5);
}

/*TEST_CASE("diff: minimize rosenbrock", "[diff]") {
    auto init = std::make_shared<NormalInit>(1, 0);
    auto x = Param({1}, "x", init);
    auto y = Param({1}, "y", init);
    Minimize(100*Sqr(y - x*x) + Sqr(1 - x), 0.001, 2000);

    REQUIRE(abs(x->v[0] - 1) < 1e-5);
    REQUIRE(abs(y->v[0] - 1) < 1e-5);
}*/

#if 0
    Optimize("easom", 100, return -cos(x)*cos(y)*exp(-sqr(x - PI) - sqr(y - PI)));
    Optimize("himmelblau", 5, return sqr(x*x + y - 11) + sqr(x + y*y - 7));
    Optimize("styblinski–tang", 5, return x*x*x*x - 16*x*x + 5*x + y*y*y*y - 16*y*y + 5*y);
    Optimize("schaffer n.2", 100, return 0.5 + (sqr(sin(x*x - y*y)) - 0.5) / sqr(1 + 0.001*(x*x + y*y)));
    Optimize("hölder table", 10, return -abs(sin(x)*cos(y)*exp(abs(1 - sqrt(x*x + y*y)/PI))));
#endif

TEST_CASE("diff: learn perceptron, plane in 2d", "[diff]") {
    constexpr bool D3 = false;
    parallel(5, [&](size_t seed) {
        auto x = Data({}) << "x";
        auto y = Data({}) << "y";
        auto z = Data({}) << "y";
        auto ref = Data({}) << "ref";

        std::mt19937_64 random(seed);
        auto init = std::make_shared<NormalInit>(1 / sqrt(2), random);
        auto a = Param({}, init) << "a";
        auto b = Param({}, init) << "b";
        auto c = Param({}) << "d";
        auto d = D3 ? Param({}, init) << "c" : nullptr;

        auto fc = x * a + y * b + c;
        if (D3) fc = fc + z * d;
        auto out = Logistic(fc, 15) << "out";
        auto loss = BinaryCrossEntropy(ref, out) << "loss";
        auto accuracy = BinaryAccuracy(ref, out);

        Model model({loss, accuracy});
        model.SetBatchSize(24);
        model.optimizer = std::make_shared<Adam>();
        model.optimizer->alpha = 0.1;

        // dataset
        const float A = 0.2, B = 0.4, C = -0.8, D = 0.1;
        UniformInit gen(-1, 1, random);
        const int Classes = D3 ? 3 : 2;
        const int SamplesPerClass = 20000 - 8;
        const int Samples = Classes * SamplesPerClass;
        vtensor data_x({Samples}, 0);
        vtensor data_y({Samples}, 0);
        vtensor data_z({Samples}, 0);
        vtensor data_r({Samples}, 0);
        int count[2] = {0, 0};
        int index = 0;
        while (index < Samples) {
            float dx = gen.get();
            float dy = gen.get();
            float dz = D3 ? gen.get() : 0;
            float dr = (dx * A + dy * B + dz * C + D >= 0) ? 1 : 0;
            int c = round(dr);
            if (count[c] >= SamplesPerClass) continue;
            count[c] += 1;
            data_x[index] = dx;
            data_y[index] = dy;
            if (D3) data_z[index] = dz;
            data_r[index] = dr;
            index += 1;
        }
        std::vector<std::pair<Diff, tensor>> dataset;
        if (D3) dataset = {{x, data_x}, {y, data_y}, {z, data_z}, {ref, data_r}};
        if (!D3) dataset = {{x, data_x}, {y, data_y}, {ref, data_r}};
        NormalizeDataset(data_x);
        NormalizeDataset(data_y);
        if (D3) NormalizeDataset(data_z);

        // train!
        Metrics metrics;
        FOR(i, 1000) metrics = model.Epoch(loss, accuracy, dataset, random, false, i);
        REQUIRE(metrics.at("accuracy") >= 0.9994);
        fmt::print("accuracy: {}\n", metrics.at("accuracy"));
    });
}

Diff Neuron(Diff x, Diff y, std::shared_ptr<Init> init) {
    auto a = Param({}, init) << "a";
    auto b = Param({}, init) << "b";
    auto c = Param({}) << "c";
    return Logistic(x * a + y * b + c, 15);
}

Diff Neuron(Diff x, Diff y, Diff z, std::shared_ptr<Init> init) {
    auto a = Param({}, init) << "a";
    auto b = Param({}, init) << "b";
    auto c = Param({}, init) << "c";
    auto d = Param({}) << "d";
    return Logistic(x * a + y * b + z * c + d, 15);
}

// Effect of batch size on duration and accuracy
// B16 -> 11.7s 99.17%
// B20 -> 10.9s 99.23%
// B24 -> 10.5s 99.23%
// B32 -> 8.7s 99.19%
// B64 -> 7.8s 98.98%
// B128 -> 7.2s 98.43%
// B256 -> 7.0s 97.39%
// B512 -> 6.9s 95.28%
TEST_CASE("diff: learn two layer network, circle in 2d", "[diff]") {
    parallel(5, [&](size_t seed) {
        auto x = Data({}) << "x";
        auto y = Data({}) << "y";
        auto ref = Data({}) << "ref";

        std::mt19937_64 random(seed);
        auto init = std::make_shared<NormalInit>(1, random);
        auto h1 = Neuron(x, y, init) << "h1";
        auto h2 = Neuron(x, y, init) << "h2";
        auto h3 = Neuron(x, y, init) << "h3";
        auto init2 = std::make_shared<NormalInit>(1, random);
        auto out = Neuron(h1, h2, h3, init2) << "out";
        auto loss = MeanSquareError(ref, out) << "loss";
        auto accuracy = BinaryAccuracy(ref, out);
        Model model({loss, accuracy});
        model.SetBatchSize(24);
        model.optimizer = std::make_shared<RMSProp>();
        model.optimizer->alpha = 0.001;

        // dataset
        UniformInit gen(-1, 1, random);
        const int Classes = 2;
        const int SamplesPerClass = 20000 - 8;
        const int Samples = Classes * SamplesPerClass;
        vtensor data_x({Samples}, 0);
        vtensor data_y({Samples}, 0);
        vtensor data_r({Samples}, 0);
        int count[2] = {0, 0};
        int index = 0;
        while (index < Samples) {
            float dx = gen.get();
            float dy = gen.get();
            float dr = (dx * dx + dy * dy >= 0.5) ? 1 : 0;
            int c = round(dr);
            if (count[c] >= SamplesPerClass) continue;
            count[c] += 1;
            data_x[index] = dx;
            data_y[index] = dy;
            data_r[index] = dr;
            index += 1;
        }
        std::vector<std::pair<Diff, tensor>> dataset = {{x, data_x}, {y, data_y}, {ref, data_r}};

        // train!
        Metrics metrics;
        FOR(i, 1000) metrics = model.Epoch(loss, accuracy, dataset, random, false, i);
        fmt::print("accuracy: {}\n", metrics.at("accuracy"));
#ifdef linux
        REQUIRE(metrics.at("accuracy") >= 0.77);
#else
        REQUIRE(metrics.at("accuracy") >= 0.9943);
#endif
    });
}

TEST_CASE("diff: learn FC perceptron, hyperplane", "[diff]") {
    constexpr int N = 2;
    parallel(5, [&](size_t seed) {
        auto in = Data({N}) << "in";
        auto ref = Data({}) << "ref";

        std::mt19937_64 random(seed);
        auto init = std::make_shared<NormalInit>(1, random);
        auto fc = FullyConnected(in, 1, init);
        auto out = Logistic(fc, 15) << "out";
        auto loss = BinaryCrossEntropy(ref, out) << "loss";
        auto accuracy = BinaryAccuracy(ref, out) << "accuracy";

        Model model({loss, accuracy});
        model.SetBatchSize(24);
        model.optimizer = std::make_shared<Adam>();
        model.optimizer->alpha = 0.1;

        // dataset
        UniformInit gen(-1, 1, random);
        std::vector<float> W;
        FOR(i, N) W.push_back(gen.get() / 2);
        float B = gen.get() / 10;

        const int Classes = 2;
        const int SamplesPerClass = 20000 - 8;
        const int Samples = Classes * SamplesPerClass;
        vtensor data_in({Samples, N}, 0);
        vtensor data_ref({Samples}, 0);
        int count[2] = {0, 0};
        int index = 0;
        std::vector<float> d;
        while (index < Samples) {
            d.clear();
            FOR(i, N) d.push_back(gen.get());
            float s = B;
            FOR(i, N) s += d[i] * W[i];
            float dr = (s >= 0) ? 1 : 0;
            int c = round(dr);
            if (count[c] >= SamplesPerClass) continue;
            count[c] += 1;
            FOR(i, N) data_in(index, i) = d[i];
            data_ref[index] = dr;
            index += 1;
        }
        std::vector<std::pair<Diff, tensor>> dataset = {{in, data_in}, {ref, data_ref}};
        NormalizeDataset(data_in);

        // train!
        Metrics metrics;
        FOR(i, 1000) metrics = model.Epoch(loss, accuracy, dataset, random, false, i);
        fmt::print("accuracy: {}\n", metrics.at("accuracy"));
#ifdef linux
        REQUIRE(metrics.at("accuracy") >= 0.9993);
#else
        REQUIRE(metrics.at("accuracy") >= 0.9994);
#endif
    });
}

// TODO train / test datasets should be fixed an not depend on seed!
// TODO seed is for weight initialization only!
// TODO evaluate on separate test set afterwards!
TEST_CASE("diff: learn FC two layer network, circle in 2d", "[diff]") {
    parallel(5, [&](size_t seed) {
        auto in = Data({2}) << "in";
        auto ref = Data({}) << "ref";

        std::mt19937_64 random(seed);
        auto init_1 = std::make_shared<NormalInit>(0.333f, random);
        auto init_2 = std::make_shared<NormalInit>(1, random);

        auto x = in;
        x = FullyConnected(x, 3, init_1) << "fc";
        x = Tanh(x);
        x = FullyConnected(x, 1, init_2);
        x = Logistic(x, 15);

        auto out = Reshape(x, {}) << "out";
        auto loss = MeanSquareError(ref, out) << "loss";
        auto accuracy = BinaryAccuracy(ref, out) << "accuracy";

        Model model({loss, accuracy});
        model.SetBatchSize(24);
        model.optimizer = std::make_shared<RMSProp>();
        model.optimizer->alpha = 0.001;

        // dataset
        UniformInit gen(-1, 1, random);
        const int Classes = 2;
        const int SamplesPerClass = 20000 - 8;
        const int Samples = Classes * SamplesPerClass;
        vtensor data_in({Samples, 2}, 0);
        vtensor data_ref({Samples}, 0);
        int count[2] = {0, 0};
        int index = 0;
        while (index < Samples) {
            float dx = gen.get();
            float dy = gen.get();
            float dr = (dx * dx + dy * dy >= 0.5) ? 1 : 0;
            int c = round(dr);
            if (count[c] >= SamplesPerClass) continue;
            count[c] += 1;
            data_in(index, 0) = dx;
            data_in(index, 1) = dy;
            data_ref[index] = dr;
            index += 1;
        }
        std::vector<std::pair<Diff, tensor>> dataset = {{in, data_in}, {ref, data_ref}};
        NormalizeDataset(data_in);

        // train!
        Metrics metrics;
        FOR(i, 1000) metrics = model.Epoch(loss, accuracy, dataset, random, false, i);
        fmt::print("accuracy: {}\n", metrics.at("accuracy"));
#ifdef linux
        REQUIRE(metrics.at("accuracy") >= 0.9921);
#else
        REQUIRE(metrics.at("accuracy") >= 0.9930);
#endif
    });
}

// TODO not complete!
TEST_CASE("diff: Polynomial, learn sin(x)", "[diff_x][.]") {
    const int Batch = env("batch", 100);
    const std::vector<float> poly = {0, 1, 0, -1.f/(2*3), 0, 1.f/(2*3*4*5)};
    parallel(1, [&](size_t seed) {
        auto in = Data({1}) << "in";
        auto ref = Data({1}) << "ref";

        std::mt19937_64 random(seed);
        auto init = std::make_shared<NormalInit>(0.1f, random);

        //auto out = Reshape(Polynomial(in, 5, 1, init), {}) << "out";
        auto x = in;
        auto out = Param({}, init) << "a";
        //auto b = Param({}, init) << "b";
        out += /*b **/ x;
        out += (Param({}, init) << "c") * x * x;
        out += (Param({}, init) << "d") * x * x * x;
        out += (Param({}, init) << "e") * x * x * x * x;
        out += (Param({}, init) << "f") * x * x * x * x * x;
        //auto loss = MeanSquareError(ref, out) /*+ Sqr(b - 1)*/ << "loss";
        auto diff = Abs(ref - out);
        auto loss = Mean(diff * diff * diff) << "loss";
        auto mse = MeanSquareError(ref, out) << "mse";

        Model model({loss, mse});
        model.SetBatchSize(Batch);
        model.optimizer = std::make_shared<RMSProp>();
        model.optimizer->alpha = env("alpha", 0.0001);

        // dataset
        UniformInit gen(-0.5, 0.5, random);
        const dim_t Samples = RoundUp(20000, Batch);
        vtensor data_in({Samples, 1}, 0);
        vtensor data_ref({Samples, 1}, 0);
        int index = 0;
        while (index < Samples) {
            float dx = gen.get();
            data_in[index] = dx;
            data_ref[index] = ComputePolynomial(dx, poly);

            index += 1;
        }
        std::vector<std::pair<Diff, tensor>> dataset = {{in, data_in}, {ref, data_ref}};
        NormalizeDataset(data_in);

        // train!
        Metrics metrics;
        FOR(i, 1000) {
            metrics = model.Epoch(loss, mse, dataset, random, true, i);
        }
        model.Print();
        //println("loss: %s", metrics.at("loss"));
        //REQUIRE(metrics.at("loss") >= 0.0);
    });
}

// TODO move to tensor utils
/*#include <opencv2/opencv.hpp>

uchar ToByte(tensor::type a) {
    int color = std::floor(a * 255);
    if (color > 255) color = 255;
    if (color < 0) color = 0;
    return color;
}

void WriteColorImage(tensor in, std::string path) {
    Check(in.ndims() == 3);
    Check(in.dim(2) == 3);
    cv::Mat image = cv::Mat::zeros(in.dim(1), in.dim(0), CV_8UC3);
    FOR(x, in.dim(0)) FOR(y, in.dim(1)) {
        auto& pixel = image.at<cv::Vec3b>(y, x);
        static_assert(sizeof(pixel) == 3);
        pixel[0] = ToByte(in(x, y, 0));
        pixel[1] = ToByte(in(x, y, 1));
        pixel[2] = ToByte(in(x, y, 2));
    }
    imwrite(path, image);
}

void WriteGrayImage(tensor in, std::string path) {
    Check(in.ndims() == 3);
    Check(in.dim(2) == 1);
    cv::Mat image = cv::Mat::zeros(in.dim(1), in.dim(0), CV_8UC1);
    FOR(x, in.dim(0)) FOR(y, in.dim(1)) {
        auto& pixel = image.at<uchar>(y, x);
        pixel = ToByte(in(x, y, 0));
    }
    imwrite(path, image);
}

vtensor ReadColorImage(std::string path) {
    cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
    println("rows %s, cols %s", image.rows, image.cols);
    vtensor out({dim_t(image.cols), dim_t(image.rows), 3, 0, 'w', 'h', 'c'});
    Check(out.ndims() == 3);
    Check(out.dim(2) == 3);
    FOR(x, out.dim(0)) FOR(y, out.dim(1)) {
        auto& pixel = image.at<cv::Vec3b>(y, x);
        out(x, y, 0) = double(pixel[0]) / 255;
        out(x, y, 1) = double(pixel[1]) / 255;
        out(x, y, 2) = double(pixel[2]) / 255;
    }
    return out;
}

vtensor ReadGrayImage(std::string path) {
    cv::Mat image = cv::imread(path, cv::IMREAD_GRAYSCALE);
    println("rows %s, cols %s", image.rows, image.cols);
    vtensor out({dim_t(image.cols), dim_t(image.rows), 1, 0, 'w', 'h', 'c'});
    FOR(x, out.dim(0)) FOR(y, out.dim(1)) {
        auto& pixel = image.at<uchar>(y, x);
        out(x, y, 0) = double(pixel) / 255;
    }
    return out;
}

TEST_CASE("diff: Conv2D, testing forward", "[diff_cf]") {
    vtensor image = ReadGrayImage("/Users/marko/Desktop/landscape.png");
    image.reshape(image.shape().push_front(1, 'b'));
    auto a = Param(image.shape());
    std::swap(a->v, image);
    auto k = Param({1, 3, 3, 1, 'i', 'w', 'h', 'c'});
    FOR(x, 3) FOR(y, 3) k->v(0, x, y, 0) = int(x) - 1;
    auto conv = Conv2D(a, ConvType::Same, k);
    conv->Forward(true);
    WriteGrayImage(conv->v.slice(0), "/Users/marko/Desktop/landscape_conv.png");
}*/

TEST_CASE("diff: Conv2D, find dots", "[diff_c]") {
    const int Batch = env("batch", 10);
    parallel(env("nets", 5), [&](size_t seed) {
        const dim_t N = env("n", 16);
        const dim4 s(N, N, 1, 0, 'w', 'h', 'c');
        auto in = Data(s) << "in";
        auto ref = Data(s) << "ref";

        std::mt19937_64 random(seed);
        auto init = std::make_shared<NormalInit>(env("init", 0.1), random);

        auto x = Conv2D(in, ConvType::Same, {1, 3, 3, 1, 'i', 'w', 'h', 'c'}, init);
        auto out = Logistic(x, 15);

        auto loss = BinaryCrossEntropy(ref, out) << "loss";
        auto accuracy = BinaryAccuracy(ref, out) << "accuracy";

        Model model({loss, accuracy});
        model.SetBatchSize(Batch);
        model.optimizer = std::make_shared<Adam>();
        model.optimizer->alpha = env("alpha", 0.01);

        // dataset
        UniformInit gen(0.0, 1.0, random);
        const dim_t Samples = RoundUp(env<int>("samples", 1000), Batch);
        vtensor data_in(s.push_front(Samples, 'b'), 0);
        vtensor data_ref(s.push_front(Samples, 'b'), 0);
        FOR(index, Samples) {
            tensor din = data_in.slice(index);
            FOR(x, N) FOR(y, N) din(x, y, 0) = gen.get() <= 0.05;
            //WriteGrayImage(din, format("in/%d.png", index));

            tensor dref = data_ref.slice(index);
            FOR(x, N) FOR(y, N) {
                bool clear = true;
                for (int dx : {-1, 0, 1}) for (int dy : {-1, 0, 1})
                    if (dx != 0 || dy != 0)
                        if (0 <= int(x) + dx && int(x) + dx < N && 0 <= int(y) + dy && int(y) + dy < N)
                            if (din(x + dx, y + dy) == 1)
                                clear = false;
                dref(x, y, 0) = (din(x, y, 0) == 1) && clear;
            }
            //WriteGrayImage(dref, format("ref/%d.png", index));
        }
        std::vector<std::pair<Diff, tensor>> dataset = {{in, data_in}, {ref, data_ref}};
        NormalizeDataset(data_in);

        model.Epoch(loss, accuracy, dataset, random, true, 0, false);
        model.Print();

        // train!
        Metrics metrics;
        FOR(i, env<int>("epochs", 1000)) {
            metrics = model.Epoch(loss, accuracy, dataset, random, env<int>("verbose", 1), i);
        }
        model.Print();
        fmt::print("loss: {}\n", metrics.at("loss"));
        //REQUIRE(metrics.at("loss") >= 0.0);
    });
}

// TODO Kronecker learn x*y

// Classification:
// learn hyper plane in Nd
// learn hyper sphere in Nd
// spiral in 2d

// Neuro-plasticity:
// 1) train same model on datasets A and B separately, then take model trained of A and train it on B, should be able to
// get to similar performance level as when only trained on B 2) train fully on A, then fully on B, then some refresher
// on A, and make sure that both skills are retained

// Be able to detect if input is 1) in-domain (ie. similar to training data) or 2) out-of-domain (very different from
// training data)

// Idea: add 2nd order derivatives!
// Idea: solve motion planning problems! One layer/diff for one time step. All costs add up to loss. Parameters are
// control inputs! Idea: could take advantage of mini batches to solve multiple problems at once. Idea: would need to be
// able to compute Lp-distance on GPU

// Idea: let model decide if it wants to propagate gradients to any specific part of the network
