//------------------------------------------------------------------------------
//  DrawCallExplorer.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Core/App.h"
#include "Gfx/Gfx.h"
#include "Assets/Gfx/ShapeBuilder.h"
#include "Input/Input.h"
#include "Time/Clock.h"
#include "IMUI/IMUI.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp"
#include "shaders.h"

using namespace Oryol;

class DrawCallExplorerApp : public App {
public:
    AppState::Code OnRunning();
    AppState::Code OnInit();
    AppState::Code OnCleanup();
    
private:
    void updateCamera();
    void emitParticles();
    void updateParticles();
    void drawUI();

    int maxNumParticles = 10000;
    int numEmitParticles = 100;
    int curNumParticles = 0;
    int numParticlesPerBatch = 1000;

    StaticArray<Id,3> drawStates;

    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;

    // FIXME: hmm these param blocks are actually compatibel across shaders
    Shaders::Red::PerFrameParams perFrameParams;
    Shaders::Red::PerParticleParams perParticleParams;

    static const int ParticleBufferSize = 1000000;
    bool updateEnabled = true;
    int32 frameCount = 0;
    TimePoint lastFrameTimePoint;
    struct {
        glm::vec4 pos;
        glm::vec4 vec;
    } particles[ParticleBufferSize];
};
OryolMain(DrawCallExplorerApp);

//------------------------------------------------------------------------------
AppState::Code
DrawCallExplorerApp::OnRunning() {
    
    Duration updTime, drawTime, applyRtTime;
    this->frameCount++;
    
    // update block
    this->updateCamera();
    if (this->updateEnabled) {
        TimePoint updStart = Clock::Now();
        this->emitParticles();
        this->updateParticles();
        updTime = Clock::Since(updStart);
    }
    
    // render block
    TimePoint applyRtStart = Clock::Now();
    Gfx::ApplyDefaultRenderTarget(ClearState::ClearAll(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f, 0));
    applyRtTime = Clock::Since(applyRtStart);
    TimePoint drawStart = Clock::Now();
    int batchCount = this->numParticlesPerBatch;
    int curBatch = 0;
    for (int32 i = 0; i < this->curNumParticles; i++) {
        if (++batchCount > this->numParticlesPerBatch) {
            batchCount = 0;
            Gfx::ApplyDrawState(this->drawStates[curBatch++]);
            Gfx::ApplyUniformBlock(this->perFrameParams);
            if (curBatch >= 3) {
                curBatch = 0;
            }
        }
        this->perParticleParams.Translate = this->particles[i].pos;
        Gfx::ApplyUniformBlock(this->perParticleParams);
        Gfx::Draw(0);
    }
    drawTime = Clock::Since(drawStart);

    this->drawUI();
    Gfx::CommitFrame();

    return Gfx::QuitRequested() ? AppState::Cleanup : AppState::Running;
}

//------------------------------------------------------------------------------
void
DrawCallExplorerApp::updateCamera() {
    float32 angle = this->frameCount * 0.01f;
    glm::vec3 pos(glm::sin(angle) * 10.0f, 2.5f, glm::cos(angle) * 10.0f);
    this->view = glm::lookAt(pos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    this->perFrameParams.ModelViewProjection = this->proj * this->view * this->model;
}

//------------------------------------------------------------------------------
void
DrawCallExplorerApp::emitParticles() {
    for (int32 i = 0;
        (this->curNumParticles < this->maxNumParticles) &&
        (i < this->numEmitParticles);
        i++) {
        
        this->particles[this->curNumParticles].pos = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 rnd = glm::ballRand(0.5f);
        rnd.y += 2.0f;
        this->particles[this->curNumParticles].vec = glm::vec4(rnd, 0.0f);
        this->curNumParticles++;
    }
}

//------------------------------------------------------------------------------
void
DrawCallExplorerApp::updateParticles() {
    const float32 frameTime = 1.0f / 60.0f;
    for (int32 i = 0; i < this->curNumParticles; i++) {
        auto& curParticle = this->particles[i];
        curParticle.vec.y -= 1.0f * frameTime;
        curParticle.pos += curParticle.vec * frameTime;
        if (curParticle.pos.y < -2.0f) {
            curParticle.pos.y = -1.8f;
            curParticle.vec.y = -curParticle.vec.y;
            curParticle.vec *= 0.8f;
        }
    }
}

//------------------------------------------------------------------------------
AppState::Code
DrawCallExplorerApp::OnInit() {
    // setup rendering system
    GfxSetup gfxSetup = GfxSetup::Window(800, 500, "Oryol DrawCallPerf Sample");
    gfxSetup.GlobalUniformBufferSize = 1024 * 1024 * 32;
    Gfx::Setup(gfxSetup);
    Input::Setup();
    Input::BeginCaptureText();
    IMUI::Setup();

    // create resources
    const glm::mat4 rot90 = glm::rotate(glm::mat4(), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    ShapeBuilder shapeBuilder;
    shapeBuilder.RandomColors = true;
    shapeBuilder.Layout
        .Add(VertexAttr::Position, VertexFormat::Float3)
        .Add(VertexAttr::Color0, VertexFormat::Float4);
    shapeBuilder.Transform(rot90).Sphere(0.05f, 3, 2);
    Id mesh = Gfx::CreateResource(shapeBuilder.Build());

    Id redShd   = Gfx::CreateResource(Shaders::Red::Setup());
    Id greenShd = Gfx::CreateResource(Shaders::Green::Setup());
    Id blueShd  = Gfx::CreateResource(Shaders::Blue::Setup());

    auto dss = DrawStateSetup::FromMeshAndShader(mesh, redShd);
    dss.RasterizerState.CullFaceEnabled = true;
    dss.DepthStencilState.DepthWriteEnabled = true;
    dss.DepthStencilState.DepthCmpFunc = CompareFunc::LessEqual;
    this->drawStates[0] = Gfx::CreateResource(dss);
    dss.Shader = greenShd;
    this->drawStates[1] = Gfx::CreateResource(dss);
    dss.Shader = blueShd;
    this->drawStates[2] = Gfx::CreateResource(dss);
    
    // setup projection and view matrices
    const float32 fbWidth = (const float32) Gfx::DisplayAttrs().FramebufferWidth;
    const float32 fbHeight = (const float32) Gfx::DisplayAttrs().FramebufferHeight;
    this->proj = glm::perspectiveFov(glm::radians(45.0f), fbWidth, fbHeight, 0.01f, 100.0f);
    this->view = glm::lookAt(glm::vec3(0.0f, 2.5f, 0.0f), glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    this->model = glm::mat4();
    
    return App::OnInit();
}

//------------------------------------------------------------------------------
AppState::Code
DrawCallExplorerApp::OnCleanup() {
    IMUI::Discard();
    Input::Discard();
    Gfx::Discard();
    return App::OnCleanup();
}

//------------------------------------------------------------------------------
void
DrawCallExplorerApp::drawUI() {
    IMUI::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(240, 200), ImGuiSetCond_Once);
    if (ImGui::Begin("Controls")) {
        if (ImGui::Button("Reset")) {
            this->curNumParticles = 0;
        }
        ImGui::PushItemWidth(100.0f);
        if (ImGui::InputInt("Max Particles", &this->maxNumParticles, 100, 1000, ImGuiInputTextFlags_EnterReturnsTrue)) {
            this->curNumParticles = 0;
        }
        if (ImGui::InputInt("Batch Size", &this->numParticlesPerBatch, 10, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
            this->curNumParticles = 0;
        }
        ImGui::InputInt("Emit Per Frame", &this->numEmitParticles, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
    }
    ImGui::End();
    ImGui::Render();

}