#include "Engine.h"

// ============================================================================
// Standard library
// ============================================================================
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <type_traits>

// ============================================================================
// OpenGL
// ============================================================================
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ============================================================================
// Math
// ============================================================================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

// ============================================================================
// Third party
// ============================================================================
#include <ft2build.h>
#include FT_FREETYPE_H

// ============================================================================
// Engine — core
// ============================================================================
#include "common.h"
#include "Window.h"
#include "Camera.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Physics.h"
#include "RenderSystem.h"
#include "SoundSystem.h"
#include "ResourceManager.h"
#include "tFunctions.h"
#include "Debug.h"

// Rendering
#include "shaderClass.h"
#include "VBO.h"
#include "VAO.h"
#include "EBO.h"
#include "FBO.h"
#include "texture.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "Cubemap.h"
#include "QuadVertices.h"
#include "Particle.h"
#include "Lighting.h"
#include "DirLight.h"

// Scene
#include "Folder.h"
#include "Sound.h"
#include "Gui.h"
#include "font.h"

// Character / animation
#include "Rig.h"
#include "Animator.h"
#include "Character.h"
#include "DefaultBodyComponents.h"
#include "BodyComponentActions.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ============================================================================
// Constructor — window, callbacks, systems, shaders
// ============================================================================
Engine::Engine()
{
    engine = this;

    glfwSetFramebufferSizeCallback(window->handle, framebuffer_size_callback);
    glfwSetCursorPosCallback(window->handle, mouse_callback);
    glfwSetScrollCallback(window->handle, scroll_callback);
    glfwSetKeyCallback(window->handle, key_callback);
    glfwSetMouseButtonCallback(window->handle, mouse_button_callback);
    glfwSwapInterval(1);

    rendersystem = new RenderSystem();
    rendersystem->Initialize();
    physicsengine = new Physics();
    mainf = new Folder();
    camera = new Camera();
    camera->t.TranslateTo({ 0.0f, 5.0f, 0.0f });

    rendersystem->RigShader = new Shader("shaders/rig.vert", "shaders/default.frag");
    rendersystem->MeshShader = new Shader("shaders/default.vert", "shaders/default.frag");
    rendersystem->MeshShadowShader = new Shader("shaders/meshshadow.vert", "shaders/shadow.frag");
    rendersystem->RigShadowShader = new Shader("shaders/rigshadow.vert", "shaders/shadow.frag");
    rendersystem->SkyboxShader = new Shader("shaders/skybox.vert", "shaders/skybox.frag");
    rendersystem->ScreenShader = new Shader("shaders/ScreenShader.vert", "shaders/ScreenShader.frag");
    rendersystem->ImageBoxShader = new Shader("shaders/ImageBox.vert", "shaders/ImageBox.frag");
    rendersystem->BoxShader = new Shader("shaders/Box.vert", "shaders/Box.frag");
    rendersystem->Text2DShader = new Shader("shaders/2DText.vert", "shaders/2DText.frag");
    rendersystem->ParticleShader = new Shader("shaders/Particle.vert", "shaders/Particle.frag");
}

// ============================================================================
// Initiate — scene setup + game loop
// ============================================================================
void Engine::Initiate()
{
    // ------------------------------------------------------------------------
    // Assets
    // ------------------------------------------------------------------------
    Font* arial = new Font("assets/ft/arial.ttf");
    Texture* blanktex = new Texture("assets/blanktexture.png");
    Texture* magic = new Texture("assets/itsmagicbitch.jpg");
    Texture* snowflake = new Texture("assets/snowflake.png");
    Texture* leontex = new Texture("assets/revolvertex.png");

    // ------------------------------------------------------------------------
    // Shared rig + animations
    // ------------------------------------------------------------------------
    Rig* testrig = new Rig("assets/rig.glb");
    Animation* testanimation = new Animation("assets/walkanim.glb");
    Animation* punchanim = new Animation("assets/punchanim.glb");

    // ------------------------------------------------------------------------
    // Player character
    // ------------------------------------------------------------------------
    Character* testcharacter = new Character();
    testcharacter->name = "character";
    testcharacter->castshadow = true;
    testcharacter->body->t.ScaleBy({ 0.5f, 1.8f, 0.5f });
    testcharacter->body->t.TranslateTo({ 0.0f, 0.9f, 0.0f });
    testcharacter->body->SetRig(testrig);

    // Base animator (weight 0) — walk, looped
    testcharacter->body->SetAnimation(testanimation);
    testcharacter->body->animator->loops = true;

    // Punch animator (weight 1) — one-shot, starts stopped
    Animator* punchAnimator = testcharacter->body->AddAnimator(1);
    punchAnimator->loops = false;
    punchAnimator->SetAnimation(punchanim);
    punchAnimator->StopAnimation();

    testcharacter->BindToRenderSystem();

    p* characterBody = new p(testcharacter->body);
    characterBody->mass = 80.0f;
    characterBody->lockRotation = true;
    characterBody->friction = 0.0f;
    physicsengine->AddObject(characterBody);

    // ------------------------------------------------------------------------
    // NPC
    // ------------------------------------------------------------------------
    Character* npc = new Character();
    npc->name = "npc";
    npc->castshadow = true;
    npc->body->t.ScaleBy({ 0.5f, 1.8f, 0.5f });
    npc->body->t.TranslateTo({ 3.0f, 0.9f, 3.0f });
    npc->body->SetRig(testrig);
    npc->body->SetAnimation(testanimation);
    npc->BindToRenderSystem();

    p* npcBody = new p(npc->body);
    npcBody->mass = 80.0f;
    npcBody->lockRotation = true;
    npcBody->friction = 0.0f;
    physicsengine->AddObject(npcBody);

    // ------------------------------------------------------------------------
    // Rain particles
    // ------------------------------------------------------------------------
    ParticleEmitter* rain = new ParticleEmitter();
    rain->name = "rain";
    rain->tex = snowflake;
    rain->opaque = false;
    rain->speed = 35.0f;
    rain->lifespan = 0.6f;
    rain->angularvelocity = { 0.0f, 0.0f, glm::radians(10.0f) };
    rain->size = { 0.04f, 0.4f, 0.0f };
    rain->emitangle = { glm::radians(60.0f), glm::radians(360.0f), glm::radians(60.0f) };
    rain->facecamera = false;
    rain->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    rain->emitdirection = ParticleEmitter::EmitDirection::Perpendicular;
    rain->t.TranslateTo({ 0.0f, 10.0f, 0.0f });
    rain->t.RotateToQuaternion(glm::quat(glm::vec3(glm::radians(85.0f), 0.0f, 0.0f)));
    rain->BindToRenderSystem();
    mainf->AddChild(rain);

    // ------------------------------------------------------------------------
    // 3D audio
    // ------------------------------------------------------------------------
    Sound* music = new Sound();
    music->name = "music";
    music->LoadSound("assets/fs.wav", true);
    music->Update3DPosition(0.0f, 0.0f, 0.0f);
    music->PlayTrack(true);
    mainf->AddChild(music);

    // ------------------------------------------------------------------------
    // GUI
    // ------------------------------------------------------------------------
    Box* crosshair = new Box();
    crosshair->Color = { 0.0f, 0.0f, 0.0f };
    crosshair->t2d.center = { 0.5f, 0.5f };
    crosshair->t2d.position = { 0.5f, 0.5f, 0.0f, 0.0f };
    crosshair->t2d.size = { 0.0f, 0.0f, 4.0f, 4.0f };
    crosshair->BindToRenderSystem();

    TextBox* tb = new TextBox();
    tb->font = arial;
    tb->text = "ASDASD\nASDASD\nADSADSA";
    tb->fontsize = 0.5f;
    tb->Color = { 0.0f, 0.0f, 0.0f };
    tb->t2d.position = { 0.5f, 0.5f, 0.0f, 0.0f };
    tb->t2d.center = { 0.0f, 0.0f };
    tb->textCenter = { 0.0f, 0.5f };
    tb->t2d.size = { 0.2f, 0.1f, 0.0f, 0.0f };
    tb->BindToRenderSystem();

    TextBox* fpsCounter = new TextBox();
    fpsCounter->font = arial;
    fpsCounter->text = "TEST1231231123";
    fpsCounter->fontsize = 0.5f;
    fpsCounter->Color = { 0.0f, 0.0f, 0.0f };
    fpsCounter->t2d.position = { 1.0f, 0.0f, 0.0f, 0.0f };
    fpsCounter->t2d.center = { 1.0f, 0.0f };
    fpsCounter->textCenter = { 0.0f, 0.5f };
    fpsCounter->t2d.size = { 0.1f, 0.1f, 0.0f, 0.0f };
    fpsCounter->BindToRenderSystem();

    ImageBox* testgui = new ImageBox();
    testgui->tex = magic;
    testgui->Color = { 1.0f, 1.0f, 1.0f };
    testgui->Opacity = 0.8f;
    testgui->rounding = 0.1f;
    testgui->t2d.center = { 0.0f, 1.0f };
    testgui->t2d.position = { 0.0f, 1.0f, 10.0f, -10.0f };
    testgui->t2d.size = { 0.0f, 0.0f, 744.0f / 2.0f, 914.0f / 2.0f };
    testgui->BindToRenderSystem();

    // ------------------------------------------------------------------------
    // World meshes
    // ------------------------------------------------------------------------
    Model leonrevolver("assets/leonrevolver.obj");
    for (Mesh* mesh : leonrevolver.meshes) {
        auto newmesh = mesh->Clone();
        newmesh->name = "revolver";
        newmesh->castshadow = true;
        newmesh->meshTexture = leontex;
        newmesh->t.TranslateBy({ 4.0f, 4.0f, 4.0f });
        newmesh->BindToRenderSystem();
        mainf->AddChild(newmesh);
    }

    Mesh* debugcube = CreateCubeMesh();
    debugcube->name = "debugcube";
    debugcube->t.ScaleTo({ 0.8f, 0.8f, 0.8f });
    debugcube->BindToRenderSystem();
    mainf->AddChild(debugcube);

    Mesh* floor = CreateCubeMesh();
    floor->name = "floor";
    floor->meshTexture = blanktex;
    floor->t.ScaleTo({ 100.0f, 2.0f, 100.0f });
    floor->t.TranslateTo({ 0.0f, -1.0f, 0.0f });
    floor->BindToRenderSystem();
    mainf->AddChild(floor);
    p* floorBody = new p(floor);
    floorBody->velocity = false;
    physicsengine->AddObject(floorBody);

    Mesh* cube = CreateCubeMesh();
    cube->name = "cube";
    cube->meshTexture = blanktex;
    cube->t.ScaleTo({ 1.0f, 1.0f, 1.0f });
    cube->t.TranslateTo({ 5.0f, 4.0f, 6.5f });
    cube->BindToRenderSystem();
    mainf->AddChild(cube);

    Mesh* cube2 = CreateCubeMesh();
    cube2->name = "cubee";
    cube2->meshTexture = blanktex;
    cube2->t.ScaleTo({ 1.0f,  1.0f,  1.0f });
    cube2->t.TranslateTo({ 10.0f, 4.0f, 10.5f });
    cube2->BindToRenderSystem();
    mainf->AddChild(cube2);

    // Ground check
    auto IsGrounded = [&]() -> bool {
        Ray downRay;
        downRay.origin = testcharacter->body->t.GetTranslation();
        downRay.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        const float halfHeight = testcharacter->body->t.GetScale().y * 0.5f;
        for (p* obj : physicsengine->GetObjects()) {
            if (obj->pointer == testcharacter->body) continue;
            if (!obj->collision) continue;
            auto hit = IsRayInT(downRay, obj->pointer->t);
            if (hit.has_value()) {
                if (glm::distance(downRay.origin, hit.value()) < halfHeight + 0.15f)
                    return true;
            }
        }
        return false;
        };

    // ========================================================================
    // Game loop
    // ========================================================================
    while (!glfwWindowShouldClose(window->handle))
    {
        frame++;
        const float deltatime = 1.0f / float(fps);

        const glm::vec3 camFront = camera->t.GetFrontVector();
        const glm::vec3 camRight = camera->t.GetRightVector();
        const float     movespeed = 3.0f;

        // --------------------------------------------------------------------
        // Input — mouse
        // --------------------------------------------------------------------
        while (Mouse::Event buffer = mouse->Read()) {
            if (buffer.GetType() == Mouse::Event::Type::Move) {
                float xoffset = mouse->GetX() - mouse->GetLastX();
                float yoffset = mouse->GetLastY() - mouse->GetY();
                camera->ProcessMouseMovement(xoffset, yoffset);
            }
            else if (buffer.GetType() == Mouse::Event::Type::RPress) {
                camera->lockedcursor = !camera->lockedcursor;
            }
        }

        // --------------------------------------------------------------------
        // Input — held movement
        // --------------------------------------------------------------------
        glm::vec3 inputVel = glm::vec3(0.0f);
        if (keyboard->IsKeyDown('W')) inputVel += glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
        if (keyboard->IsKeyDown('S')) inputVel -= glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
        if (keyboard->IsKeyDown('A')) inputVel += glm::normalize(glm::vec3(camRight.x, 0.0f, camRight.z));
        if (keyboard->IsKeyDown('D')) inputVel -= glm::normalize(glm::vec3(camRight.x, 0.0f, camRight.z));
        if (glm::length(inputVel) > 0.0f)
            inputVel = glm::normalize(inputVel) * movespeed;

        // --------------------------------------------------------------------
        // Input — keyboard events
        // --------------------------------------------------------------------
        while (Keyboard::Event buffer = keyboard->ReadKey()) {
            if (buffer.GetCode() == 'E' && buffer.IsPress()) {
                Mesh* nc = CreateCubeMesh();
                nc->name = "cube!";
                nc->t.ScaleTo(1.0f);
                nc->t.TranslateTo(camera->t.GetTranslation());
                nc->t.RotateByEulerAngles({ 0.0f, 0.0f, 0.0f });
                nc->BindToRenderSystem();
                mainf->AddChild(nc);
                physicsengine->AddObject(new p(nc));
            }
            if (buffer.GetCode() == 'F' && buffer.IsPress()) {
                const glm::vec3 playerPos = testcharacter->body->t.GetTranslation();
                const glm::vec3 playerFront = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));

                // Restart punch on high-priority animator
                punchAnimator->m_CurrentTime = 0.0f;
                punchAnimator->PlayAnimation();

                debugcube->t.TranslateTo(playerPos + playerFront * 0.8f);
                debugcube->t.ScaleTo({ 0.8f, 0.8f, 0.8f });
                debugcube->t.RotateToQuaternion(LookAt(-playerFront));

                std::optional<TInTInfo> hit = TInT(debugcube->t, npc->body->t);
                if (hit.has_value()) {
                    std::cout << "HIT NPC\n";
                    npcBody->WakeUp();
                    npcBody->linearvelocity += playerFront * 5.0f + glm::vec3(0.0f, 3.0f, 0.0f);
                    camera->AddScreenshake(0.15f);  // landing a punch feels impactful
                }
                else {
                    std::cout << "MISSED\n";
                }
            }
            if (buffer.GetCode() == GLFW_KEY_LEFT_SHIFT && buffer.IsPress()) {
                glm::vec3 dashDir = glm::length(inputVel) > 0.0f
                    ? glm::normalize(inputVel)
                    : glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
                characterBody->linearvelocity.x += dashDir.x * 18.0f;
                characterBody->linearvelocity.z += dashDir.z * 18.0f;
                camera->AddScreenshake(0.04f);
            }
            if (buffer.GetCode() == 'G' && buffer.IsPress()) {
                camera->AddScreenshake(0.2f);
            }
            if (buffer.GetCode() == GLFW_KEY_SPACE && buffer.IsPress()) {
                if (IsGrounded())
                    characterBody->linearvelocity.y = 5.0f;
            }
        }

        // --------------------------------------------------------------------
        // Input — held keys
        // --------------------------------------------------------------------
        if (keyboard->IsKeyDown(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(window->handle, 1);

        // Movement + damping
        const float accel = 40.0f;
        const float damp = 0.75f;
        characterBody->linearvelocity.x *= damp;
        characterBody->linearvelocity.z *= damp;
        characterBody->linearvelocity.x += inputVel.x * accel * deltatime;
        characterBody->linearvelocity.z += inputVel.z * accel * deltatime;

        // --------------------------------------------------------------------
        // Scene updates
        // --------------------------------------------------------------------
        {
            const glm::vec3 playerPos = testcharacter->body->t.GetTranslation();
            const glm::vec3 playerFront = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
            debugcube->t.TranslateTo(playerPos + playerFront * 0.8f);
            debugcube->t.ScaleTo({ 0.8f, 0.8f, 0.8f });
            debugcube->t.RotateToQuaternion(LookAt(-playerFront));
        }

        rain->t.TranslateTo(camera->t.GetTranslation() + glm::vec3(0.0f, 20.0f, 0.0f));
        for (int i = 0; i < 30; i++) rain->Emit();

        testcharacter->body->t.RotateToQuaternion(
            LookAt(-glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z)))
        );
        camera->t.TranslateTo(
            testcharacter->body->t.GetTranslation() - camera->t.GetFrontVector() * 3.0f
        );

        Sound* music = static_cast<Sound*>(mainf->GetFirstChildOfName("music"));
        Mesh* cube = static_cast<Mesh*>(mainf->GetFirstChildOfName("cube"));
        music->Update3DPosition(cube->t.GetTranslation());

        soundsystem->SetListenerPosition(
            camera->t.GetTranslation(),
            camera->t.GetFrontVector(),
            camera->t.GetUpVector()
        );

        // --------------------------------------------------------------------
        // FPS counter
        // --------------------------------------------------------------------
        {
            static double lastTime = glfwGetTime();
            static int    frameCount = 0;
            static int    measuredFps = 0;
            static float  accumulator = 0.0f;
            double now = glfwGetTime();
            accumulator += (float)(now - lastTime);
            lastTime = now;
            frameCount++;
            if (accumulator >= 0.25f) {
                measuredFps = (int)(frameCount / accumulator);
                frameCount = 0;
                accumulator = 0.0f;
            }
            fpsCounter->text = std::to_string(measuredFps) + " FPS";
        }

        // --------------------------------------------------------------------
        // Step systems
        // --------------------------------------------------------------------
        camera->Step(deltatime);
        rain->Step(deltatime);
        testcharacter->Step(deltatime);
        npc->Step(deltatime);
        soundsystem->Update();
        physicsengine->Step(deltatime);

        // --------------------------------------------------------------------
        // Render
        // --------------------------------------------------------------------
        rendersystem->Render(*camera);

        glfwPollEvents();
    }
}

// ============================================================================
// GLFW callbacks
// ============================================================================
void framebuffer_size_callback(GLFWwindow* window, int w, int h)
{
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)   keyboard->KeyDown(key);
    else if (action == GLFW_RELEASE) keyboard->KeyUp(key);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    mouse->OnMouseMove(static_cast<float>(xposIn), static_cast<float>(yposIn));
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPos(window, ::window->width / 2, ::window->height / 2);
    mouse->OnMouseMove(::window->width / 2.0f, ::window->height / 2.0f);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) mouse->RightDown();
        else                      mouse->RightUp();
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) mouse->LeftDown();
        else                      mouse->LeftUp();
    }
}