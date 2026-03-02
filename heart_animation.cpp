/************************************************************
 * 3D Heart Animation
 * 
 * Features:
 * - Opening: particles gather with spiral rotation
 * - Heartbeat: pulsating heart effect
 * - Ending: particles dissipate towards top-left corner
 * 
 * Environment: Visual Studio + EasyX Graphics Library
 * 
 * Code Structure:
 * - Config: centralized configuration constants
 * - Utils: helper functions (random, easing)
 * - Types: data structures
 * - Particle: particle initialization and management
 * - Animation: state-based animation logic
 * - Render: projection, color, and drawing
 * - Main: application entry point
 ************************************************************/

#include <graphics.h>
#include <conio.h>
#define NOMINMAX
#include <Windows.h>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <memory>

// ============================================================================
// Config: Configuration Constants
// ============================================================================

namespace Config {
    // Window
    constexpr int WIDTH = 800;
    constexpr int HEIGHT = 600;
    constexpr int CENTER_X = WIDTH / 2;
    constexpr int CENTER_Y = HEIGHT / 2;
    
    // Heart
    constexpr double PI = 3.14159265358979323846;
    constexpr double HEART_SCALE = 8.0;
    constexpr double DEPTH_HALF = 8.0;
    constexpr int PROFILE_SIZE = 500;
    
    // Particles
    constexpr int NUM_POINTS = 6000;
    
    // Animation Durations (seconds)
    constexpr double OPENING_DURATION = 2.5;
    constexpr double HEARTBEAT_DURATION = 3.0;
    constexpr double ENDING_DURATION = 2.0;
    
    // Projection
    constexpr double PERSPECTIVE_DIST = 500.0;
    
    // Heartbeat
    constexpr double HEARTBEAT_AMPLITUDE = 0.12;
    constexpr double HEARTBEAT_SPEED = 0.1;
    
    // Color factors for purple-blue palette
    constexpr double COLOR_R_BASE = 0.78;
    constexpr double COLOR_R_DEPTH_FACTOR = 0.1;
    constexpr double COLOR_G_BASE = 0.45;
    constexpr double COLOR_G_DEPTH_FACTOR = 0.25;
    constexpr double COLOR_B_BASE = 0.88;
    constexpr double COLOR_B_DEPTH_FACTOR = 0.07;
    
    // Brightness range
    constexpr int BRIGHTNESS_MIN = 160;
    constexpr int BRIGHTNESS_RANGE = 70;
}

// ============================================================================
// Utils: Helper Functions
// ============================================================================

namespace Utils {
    double randDouble(double min, double max) {
        return min + (max - min) * rand() / RAND_MAX;
    }
    
    double clamp(double value, double minVal, double maxVal) {
        if (value < minVal) return minVal;
        if (value > maxVal) return maxVal;
        return value;
    }
    
    int clampInt(int value, int minVal, int maxVal) {
        if (value < minVal) return minVal;
        if (value > maxVal) return maxVal;
        return value;
    }
    
    // Easing functions
    double easeOutCubic(double t) {
        return 1.0 - pow(1.0 - t, 3);
    }
    
    double easeInCubic(double t) {
        return t * t * t;
    }
    
    double easeInOutCubic(double t) {
        return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3) / 2.0;
    }
}

// ============================================================================
// Types: Data Structures
// ============================================================================

struct Point3D {
    double x, y, z;
};

struct Particle {
    Point3D start;          // Initial scattered position
    Point3D target;         // Heart shape position
    Point3D current;        // Current animated position
    Point3D dissipateTarget;// Dissipation destination
    double rotAngle;        // Rotation angle for spiral effect
    double rotSpeed;        // Rotation speed
    double delay;           // Opening animation delay
    double dissipateDelay;  // Ending animation delay
    double windStrength;    // Wind effect multiplier
};

struct RenderPoint {
    int px, py;             // Screen coordinates
    double depth;           // Z-depth for sorting
    double posX, posY;      // 3D position for color
    int brightness;         // Brightness value
    bool visible;           // Visibility flag
};

enum class AnimationState {
    Opening,
    Heartbeat,
    Ending
};

// ============================================================================
// Heart: Heart Shape Generation
// ============================================================================

namespace Heart {
    void generateProfile(double* heartX, double* heartY, int size) {
        for (int i = 0; i < size; i++) {
            double t = (double)i / size * 2 * Config::PI;
            heartX[i] = 16 * pow(sin(t), 3);
            heartY[i] = 13 * cos(t) - 5 * cos(2*t) - 2 * cos(3*t) - cos(4*t);
        }
    }
    
    bool isValidHeartPoint(double x0, double y0) {
        // Skip points too close to center vertical line
        return !(abs(x0) < 0.5 && y0 < 5.0 && y0 > -10.0);
    }
}

// ============================================================================
// Particle: Particle Initialization
// ============================================================================

namespace ParticleSystem {
    void initializeParticle(Particle& p, double x0, double y0, double scale) {
        // Target position (heart shape)
        double noise = 0.15 * Config::HEART_SCALE;
        p.target.x = x0 * scale * Config::HEART_SCALE + Utils::randDouble(-noise, noise);
        p.target.y = y0 * scale * Config::HEART_SCALE + Utils::randDouble(-noise, noise);
        p.target.z = Utils::randDouble(-Config::DEPTH_HALF, Config::DEPTH_HALF) * Config::HEART_SCALE 
                     + Utils::randDouble(-noise, noise);
        
        // Starting position (scattered)
        double angle = Utils::randDouble(0, 2 * Config::PI);
        double distance = Utils::randDouble(150, 280);
        p.start.x = distance * cos(angle);
        p.start.y = Utils::randDouble(-150, 150);
        p.start.z = distance * sin(angle) + Utils::randDouble(80, 200);
        
        p.current = p.start;
        
        // Animation properties
        p.rotAngle = Utils::randDouble(0, 2 * Config::PI);
        p.rotSpeed = Utils::randDouble(0.03, 0.1);
        p.delay = Utils::randDouble(0, 0.4);
        
        // Dissipation properties
        p.dissipateTarget.x = -Utils::randDouble(400, 600);
        p.dissipateTarget.y = Utils::randDouble(300, 500);
        p.dissipateTarget.z = Utils::randDouble(200, 400);
        
        // Position-based delay (top-left first)
        double normX = (x0 * scale + 130) / 260.0;
        double normY = (y0 * scale + 110) / 240.0;
        // Adjusted delay spread for shorter ending animation
        p.dissipateDelay = normX * 0.9 + (1.0 - normY) * 0.9 + Utils::randDouble(-0.1, 0.1);
        
        p.windStrength = Utils::randDouble(0.8, 1.2);
    }
}

// ============================================================================
// Animation: Animation State Handlers
// ============================================================================

namespace Animation {
    void updateOpening(Particle* particles, int count, double animTime) {
        double progress = animTime / Config::OPENING_DURATION;
        
        for (int i = 0; i < count; i++) {
            Particle& p = particles[i];
            
            double individualProgress = Utils::clamp(progress - p.delay, 0, 1);
            double easedProgress = Utils::easeOutCubic(individualProgress);
            
            // Update rotation
            p.rotAngle += p.rotSpeed * (1.0 - easedProgress * 0.8);
            
            // Apply rotation to start position
            double cosA = cos(p.rotAngle);
            double sinA = sin(p.rotAngle);
            
            Point3D rotatedStart;
            rotatedStart.x = p.start.x * cosA - p.start.z * sinA;
            rotatedStart.y = p.start.y;
            rotatedStart.z = p.start.x * sinA + p.start.z * cosA;
            
            // Interpolate with turbulence
            p.current.x = rotatedStart.x + (p.target.x - rotatedStart.x) * easedProgress;
            p.current.y = rotatedStart.y + (p.target.y - rotatedStart.y) * easedProgress;
            p.current.z = rotatedStart.z + (p.target.z - rotatedStart.z) * easedProgress;
            
            // Add turbulence
            double turbulence = (1.0 - easedProgress) * 8.0;
            p.current.x += sin(animTime * 8 + i * 0.1) * turbulence;
            p.current.y += cos(animTime * 6 + i * 0.15) * turbulence;
            p.current.z += sin(animTime * 7 + i * 0.12) * turbulence * 0.5;
        }
    }
    
    void updateHeartbeat(Particle* particles, int count, double t) {
        double scale = 1.0 + Config::HEARTBEAT_AMPLITUDE * sin(t);
        
        for (int i = 0; i < count; i++) {
            particles[i].current.x = particles[i].target.x * scale;
            particles[i].current.y = particles[i].target.y * scale;
            particles[i].current.z = particles[i].target.z * scale;
        }
    }
    
    void updateEnding(Particle* particles, int count, double animTime, double endingStartTime, double t) {
        double progress = (animTime - endingStartTime) / Config::ENDING_DURATION;
        double windWave = sin(animTime * 2.0) * 0.3;
        double heartbeatScale = 1.0 + Config::HEARTBEAT_AMPLITUDE * sin(t);
        
        for (int i = 0; i < count; i++) {
            Particle& p = particles[i];
            double individualProgress = progress - p.dissipateDelay;
            
            if (individualProgress <= 0) {
                // Keep heartbeat for non-dissipating particles
                p.current.x = p.target.x * heartbeatScale;
                p.current.y = p.target.y * heartbeatScale;
                p.current.z = p.target.z * heartbeatScale;
            } else {
                // Dissipate
                individualProgress = Utils::clamp(individualProgress, 0, 1);
                double easedProgress = Utils::easeInCubic(individualProgress);
                double windEffect = p.windStrength * (1.0 + windWave);
                
                double dissipateX = p.dissipateTarget.x * windEffect;
                double dissipateY = p.dissipateTarget.y * windEffect;
                double dissipateZ = p.dissipateTarget.z * windEffect;
                
                double swirl = sin(animTime * 5 + i * 0.05) * 30 * (1.0 - individualProgress);
                
                p.current.x = p.target.x + (dissipateX - p.target.x) * easedProgress + swirl;
                p.current.y = p.target.y + (dissipateY - p.target.y) * easedProgress;
                p.current.z = p.target.z + (dissipateZ - p.target.z) * easedProgress;
            }
        }
    }
}

// ============================================================================
// Render: Rendering Functions
// ============================================================================

namespace Render {
    double normalizeDepth(double z) {
        return Utils::clamp((z + 80) / 160.0, 0, 1);
    }
    
    int calculateBrightness(double z, double fadeFactor = 1.0) {
        double depthNorm = normalizeDepth(z);
        return (int)((Config::BRIGHTNESS_MIN + depthNorm * Config::BRIGHTNESS_RANGE) * fadeFactor);
    }
    
    void projectParticle(const Particle& p, RenderPoint& rp) {
        double perspective = Config::PERSPECTIVE_DIST / (Config::PERSPECTIVE_DIST + p.current.z);
        rp.px = Config::CENTER_X + (int)(p.current.x * perspective);
        rp.py = Config::CENTER_Y - (int)(p.current.y * perspective);
        rp.depth = p.current.z;
        rp.posX = p.current.x;
        rp.posY = p.current.y;
    }
    
    void calculateColor(int brightness, double depth, double posX, double posY, 
                        AnimationState state, double t, double animTime, double endingStartTime,
                        const Particle& p, int& r, int& g, int& bl) {
        double depthNorm = normalizeDepth(depth);
        double posFactor = Utils::clamp(sqrt(posX * posX + posY * posY) / 150.0, 0, 1);
        
        // Purple-blue color factors
        r = (int)(brightness * (Config::COLOR_R_BASE - depthNorm * Config::COLOR_R_DEPTH_FACTOR + posFactor * 0.05));
        g = (int)(brightness * (Config::COLOR_G_BASE + depthNorm * Config::COLOR_G_DEPTH_FACTOR));
        bl = (int)(brightness * (Config::COLOR_B_BASE + depthNorm * Config::COLOR_B_DEPTH_FACTOR - posFactor * 0.03));
        
        // Add shimmer for heartbeat and non-dissipating particles in ending
        bool applyShimmer = false;
        if (state == AnimationState::Heartbeat) {
            applyShimmer = true;
        } else if (state == AnimationState::Ending) {
            double progress = (animTime - endingStartTime) / Config::ENDING_DURATION;
            if (progress - p.dissipateDelay <= 0) {
                applyShimmer = true;
            }
        }
        
        if (applyShimmer) {
            double shimmer = sin(t * 2 + depth * 0.05) * 0.08 + 1.0;
            r = (int)(r * shimmer);
            g = (int)(g * shimmer);
            bl = (int)(bl * shimmer);
        }
        
        // Clamp
        r = Utils::clampInt(r, 0, 255);
        g = Utils::clampInt(g, 0, 255);
        bl = Utils::clampInt(bl, 0, 255);
    }
    
    int calculateRadius(AnimationState state, double animTime, double endingStartTime, 
                        const Particle& p) {
        if (state == AnimationState::Opening) {
            double progress = animTime / Config::OPENING_DURATION;
            double sizeProgress = Utils::easeInOutCubic(Utils::clamp(progress * 1.5, 0, 1));
            return (int)(3.5 * (1.0 - sizeProgress) + 2.0 * sizeProgress);
        } else if (state == AnimationState::Ending) {
            double progress = (animTime - endingStartTime) / Config::ENDING_DURATION;
            double individualProgress = progress - p.dissipateDelay;
            if (individualProgress > 0) {
                individualProgress = Utils::clamp(individualProgress, 0, 1);
                return (int)(2.5 * (1.0 - Utils::easeInCubic(individualProgress) * 0.8));
            }
        }
        return 2;
    }
    
    void updateRenderPoint(RenderPoint& rp, const Particle& p, AnimationState state,
                           double animTime, double endingStartTime) {
        projectParticle(p, rp);
        
        if (state == AnimationState::Ending) {
            double progress = (animTime - endingStartTime) / Config::ENDING_DURATION;
            double individualProgress = progress - p.dissipateDelay;
            
            if (individualProgress <= 0) {
                rp.visible = true;
                rp.brightness = calculateBrightness(p.current.z);
            } else {
                individualProgress = Utils::clamp(individualProgress, 0, 1);
                rp.visible = individualProgress < 0.9;
                rp.brightness = calculateBrightness(p.current.z, 1.0 - Utils::easeInCubic(individualProgress));
            }
        } else {
            rp.visible = true;
            rp.brightness = calculateBrightness(p.current.z);
        }
    }
    
    void depthSort(RenderPoint* points, int count) {
        std::sort(points, points + count, [](const RenderPoint& a, const RenderPoint& b) {
            return a.depth < b.depth;
        });
    }
    
    void drawParticle(const RenderPoint& rp, int r, int g, int bl, int radius) {
        setfillcolor(RGB(r, g, bl));
        solidcircle(rp.px, rp.py, radius);
    }
}

// ============================================================================
// Input: User Input Handling
// ============================================================================

namespace Input {
    bool checkExit() {
        // Check ESC key
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 27) return true;
        }
        
        // Check window close
        MSG msg;
        if (PeekMessage(&msg, GetHWnd(), 0, 0, PM_REMOVE)) {
            if (msg.message == WM_CLOSE || msg.message == WM_DESTROY) {
                return true;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        return false;
    }
}

// ============================================================================
// Main: Application Entry Point
// ============================================================================

int main()
{
    srand((unsigned)time(NULL));
    initgraph(Config::WIDTH, Config::HEIGHT);
    BeginBatchDraw();
    
    // Generate heart profile
    double heartX[Config::PROFILE_SIZE];
    double heartY[Config::PROFILE_SIZE];
    Heart::generateProfile(heartX, heartY, Config::PROFILE_SIZE);
    
    // Initialize particles
    auto particles = std::make_unique<Particle[]>(Config::NUM_POINTS);
    int validParticles = 0;
    
    while (validParticles < Config::NUM_POINTS) {
        int idx = rand() % Config::PROFILE_SIZE;
        double x0 = heartX[idx];
        double y0 = heartY[idx];
        
        if (!Heart::isValidHeartPoint(x0, y0)) continue;
        
        double z = Utils::randDouble(-1, 1);
        z = (z >= 0 ? 1 : -1) * sqrt(abs(z)) * Config::DEPTH_HALF;
        double zNorm = abs(z) / Config::DEPTH_HALF;
        double scale = sqrt(1.0 - zNorm * zNorm);
        
        ParticleSystem::initializeParticle(particles[validParticles], x0, y0, scale);
        validParticles++;
    }
    
    // Render buffer
    auto renderPoints = std::make_unique<RenderPoint[]>(Config::NUM_POINTS);
    
    // Animation state
    AnimationState state = AnimationState::Opening;
    double animTime = 0;
    double heartbeatStartTime = 0;
    double endingStartTime = 0;
    double t = 0;
    bool running = true;
    
    // Main loop
    while (running)
    {
        running = !Input::checkExit();
        if (!running) break;
        
        cleardevice();
        
        // Update animation
        switch (state) {
            case AnimationState::Opening:
                Animation::updateOpening(particles.get(), Config::NUM_POINTS, animTime);
                if (animTime >= Config::OPENING_DURATION) {
                    state = AnimationState::Heartbeat;
                    heartbeatStartTime = animTime;
                    for (int i = 0; i < Config::NUM_POINTS; i++) {
                        particles[i].current = particles[i].target;
                    }
                }
                break;
                
            case AnimationState::Heartbeat:
                Animation::updateHeartbeat(particles.get(), Config::NUM_POINTS, t);
                t += Config::HEARTBEAT_SPEED;
                if (animTime - heartbeatStartTime >= Config::HEARTBEAT_DURATION) {
                    state = AnimationState::Ending;
                    endingStartTime = animTime;
                }
                break;
                
            case AnimationState::Ending:
                Animation::updateEnding(particles.get(), Config::NUM_POINTS, animTime, endingStartTime, t);
                t += Config::HEARTBEAT_SPEED;
                if (animTime - endingStartTime >= Config::ENDING_DURATION * 3.0) {
                    running = false;
                }
                break;
        }
        
        // Update render points
        for (int i = 0; i < Config::NUM_POINTS; i++) {
            Render::updateRenderPoint(renderPoints[i], particles[i], state, animTime, endingStartTime);
        }
        
        // Depth sort
        Render::depthSort(renderPoints.get(), Config::NUM_POINTS);
        
        // Draw particles
        for (int i = 0; i < Config::NUM_POINTS; i++) {
            if (!renderPoints[i].visible) continue;
            
            int r, g, bl;
            Render::calculateColor(renderPoints[i].brightness, renderPoints[i].depth,
                                   renderPoints[i].posX, renderPoints[i].posY,
                                   state, t, animTime, endingStartTime,
                                   particles[i], r, g, bl);
            
            int radius = Render::calculateRadius(state, animTime, endingStartTime, particles[i]);
            Render::drawParticle(renderPoints[i], r, g, bl, radius);
        }
        
        FlushBatchDraw();
        Sleep(20);
        animTime += 0.02;
    }
    
    EndBatchDraw();
    closegraph();
    return 0;
}
