//
// Created by Shiina Miyuki on 2019/1/19.
//

#include "pathtracer.h"
#include "../core/scene.h"
#include "../core/sampler.h"
#include "../core/film.h"
#include "../core/geometry.h"
#include "../sampler/random.h"

using namespace Miyuki;

void PathTracer::render(Scene *scene) {
    fmt::print("Rendering\n");
    auto &film = scene->film;
    auto &seeds = scene->seeds;
    constexpr int N = 16;
    for (int i = 0; i < N; i++) {
        auto t = runtime([&]() {
            iteration(scene);
        });
        fmt::print("{}th iteration in {} secs, {} M Rays/sec\n", 1 + i, t, film.width() * film.height() / t / 1e6);
    }
}

void PathTracer::iteration(Scene *scene) {
    //TODO: refactoring, handle sampling based on BxDF tags ?
    auto &film = scene->film;
    auto &seeds = scene->seeds;
    scene->foreachPixel([&](const Point2i &pos) {
        int x = pos.x();
        int y = pos.y();
        RandomSampler randomSampler(&seeds[x + film.width() * y]);
        auto ctx = scene->getRenderContext(pos);
        Ray ray = ctx.primary;
        Spectrum radiance;
        Vec3f throughput(1, 1, 1);
        Float weightLight = 1;
        for (int depth = 0; depth < 5; depth++) {
            Intersection intersection(ray);
            intersection.intersect(scene->sceneHandle());
            if (!intersection.hit())
                break;
            ray.o += ray.d * intersection.hitDistance();
            Interaction interaction;
            scene->fetchInteraction(intersection, makeRef(&interaction));
            auto &primitive = *interaction.primitive;
            auto &material = *interaction.material;
            Vec3f wi;
            Float pdf;
            BxDFType sampledType;
            auto sample = material.sampleF(randomSampler, interaction, ray.d, &wi, &pdf, BxDFType::all, &sampledType);
            if (sampledType == BxDFType::none)break;
            if (sampledType == BxDFType::emission) {
                radiance += throughput * sample / pdf * clamp<Float>(weightLight, 0, 1);
                break;
            } else if (sampledType == BxDFType::diffuse) {
                // TODO: should we optimize the code to cancel the cosine term?
                //throughput *= sample * Vec3f::dot(interaction.norm, wi) / pdf;
                throughput *= sample;
                auto light = scene->chooseOneLight(randomSampler);
                Vec3f L;
                Float lightPdf;
                VisibilityTester visibilityTester;
                auto ka = light->sampleLi(Point2f(randomSampler.nextFloat(),
                                                  randomSampler.nextFloat()),
                                          interaction, &L, &lightPdf, &visibilityTester) * scene->lights.size();
                Float cosWi = -Vec3f::dot(L, interaction.norm);
                Float brdf = material.f(sampledType, interaction, ray.d, -1 * L);
                // balanced heuristics
                auto misPdf = pdf + lightPdf;
                weightLight = 1 / misPdf;
                if (brdf > EPS && lightPdf > 0 && cosWi > 0 && visibilityTester.visible(scene->sceneHandle())) {
                    radiance += throughput * ka / misPdf * cosWi;
                    // suppress fireflies
                    radiance = clampRadiance(radiance, 4);
                }
                throughput *= Vec3f::dot(interaction.norm, wi) / pdf;
            } else {
                assert(hasBxDFType(sampledType, BxDFType::specular));
                if (hasBxDFType(sampledType, BxDFType::glossy)) {
                    throughput *= sample;
                    auto light = scene->chooseOneLight(randomSampler);
                    Vec3f L;
                    Float lightPdf;
                    VisibilityTester visibilityTester;
                    auto ka = light->sampleLi(Point2f(randomSampler.nextFloat(),
                                                      randomSampler.nextFloat()),
                                              interaction, &L, &lightPdf, &visibilityTester) * scene->lights.size();
                    Float cosWi = -Vec3f::dot(L, interaction.norm);
                    Float brdf = material.f(sampledType, interaction, ray.d, -1 * L);
                    auto misPdf = brdf + lightPdf;
                    weightLight = 1 / misPdf;
                    if (brdf > EPS && lightPdf > 0 && cosWi > 0 && visibilityTester.visible(scene->sceneHandle())) {
                        radiance += brdf * throughput * ka / misPdf;
                        radiance = clampRadiance(radiance, 4);
                    }
                    throughput /= pdf;
                } else {
                    weightLight = 1;
                    throughput *= sample / pdf;
                }
            }
            ray.d = wi;
            if (randomSampler.nextFloat() < throughput.max()) {
                throughput /= throughput.max();
            } else {
                break;
            }
        }
        film.addSplat(pos, radiance);
    });
}