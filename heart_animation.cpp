/************************************************************
 * 3D Heart - Heart Front, Ellipse Sides
 * 
 * Features:
 * - Front view (XY plane): Heart shape
 * - Side view (YZ plane): Ellipse
 * - Top view (XZ plane): Ellipse
 * - Opening animation: particles gather from all directions
 * - Heartbeat animation after gathering
 * Environment: Visual Studio + EasyX Graphics Library
 * 
 * Optimizations:
 * - Use std::sort for O(n log n) depth sorting
 * - Pre-allocate render buffer outside loop
 * - Use smart pointers for memory management
 * - Support ESC key and window close to exit
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

// Window size
const int WIDTH = 800;
const int HEIGHT = 600;

// Heart center position
const int CENTER_X = WIDTH / 2;
const int CENTER_Y = HEIGHT / 2;

// PI constant
const double PI = 3.14159265358979323846;

// Heart scale for rendering
const double HEART_SCALE = 8.0;

// Generate random double between min and max
double randDouble(double min, double max)
{
    return min + (max - min) * rand() / RAND_MAX;
}

// Easing function: ease-out cubic for smooth deceleration
double easeOutCubic(double t)
{
    return 1.0 - pow(1.0 - t, 3);
}

// Easing function: ease-in-out cubic
double easeInOutCubic(double t)
{
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3) / 2.0;
}

// 3D Point structure
struct Point3D 
{ 
    double x, y, z; 
};

// Particle structure with animation data
struct Particle
{
    Point3D start;      // Starting position (scattered, in render coordinates)
    Point3D target;     // Target position (heart shape, in render coordinates)
    Point3D current;    // Current position (in render coordinates)
    double rotAngle;    // Individual rotation angle
    double rotSpeed;    // Rotation speed
    double delay;       // Animation delay
};

// Render point structure
struct RenderPoint 
{ 
    int px, py; 
    double depth; 
    double posX, posY;  // Store original position for color calculation
    int brightness; 
};

int main()
{
    srand((unsigned)time(NULL));
    initgraph(WIDTH, HEIGHT);
    BeginBatchDraw();

    // Precompute heart profile points
    const int PROFILE_SIZE = 500;
    double heartX[PROFILE_SIZE];
    double heartY[PROFILE_SIZE];
    
    for (int i = 0; i < PROFILE_SIZE; i++)
    {
        double t = (double)i / PROFILE_SIZE * 2 * PI;
        heartX[i] = 16 * pow(sin(t), 3);
        heartY[i] = 13 * cos(t) - 5 * cos(2*t) - 2 * cos(3*t) - cos(4*t);
    }

    // Generate particles with heart target positions
    const int NUM_POINTS = 6000;
    auto particles = std::make_unique<Particle[]>(NUM_POINTS);
    
    double depthHalf = 8.0;

    // Generate heart target positions (in render coordinates)
    for (int i = 0; i < NUM_POINTS; i++)
    {
        int idx = rand() % PROFILE_SIZE;
        double x0 = heartX[idx];
        double y0 = heartY[idx];
        
        if (abs(x0) < 0.5 && y0 < 5.0 && y0 > -10.0)
        {
            i--;
            continue;
        }
        
        double zSign = (rand() % 2 == 0) ? 1.0 : -1.0;
        double z = zSign * sqrt(randDouble(0, 1)) * depthHalf;
        
        double zNorm = abs(z) / depthHalf;
        double scale = sqrt(1.0 - zNorm * zNorm);
        
        // Target position in render coordinates (scaled for display)
        particles[i].target.x = x0 * scale * HEART_SCALE;
        particles[i].target.y = y0 * scale * HEART_SCALE;
        particles[i].target.z = z * HEART_SCALE;
        
        // Add small noise for particle effect
        double noise = 0.15 * HEART_SCALE;
        particles[i].target.x += randDouble(-noise, noise);
        particles[i].target.y += randDouble(-noise, noise);
        particles[i].target.z += randDouble(-noise, noise);
        
        // Random starting position (scattered around screen in render coordinates)
        // Smaller spread range for more intimate gathering effect
        double angle = randDouble(0, 2 * PI);
        double distance = randDouble(150, 280);
        double heightOffset = randDouble(-150, 150);
        double depthOffset = randDouble(80, 200);
        
        particles[i].start.x = distance * cos(angle);
        particles[i].start.y = heightOffset;
        particles[i].start.z = distance * sin(angle) + depthOffset;
        
        // Current position starts at start position
        particles[i].current = particles[i].start;
        
        // Individual rotation properties
        particles[i].rotAngle = randDouble(0, 2 * PI);
        particles[i].rotSpeed = randDouble(0.03, 0.1);
        
        // Staggered animation delay for wave effect
        particles[i].delay = randDouble(0, 0.4);
    }

    // Pre-allocate render buffer
    auto renderPoints = std::make_unique<RenderPoint[]>(NUM_POINTS);

    // Animation states
    enum class AnimationState { Opening, Heartbeat };
    AnimationState state = AnimationState::Opening;
    
    double animTime = 0;
    const double OPENING_DURATION = 3.5;  // Opening animation duration in seconds
    double t = 0;
    bool running = true;

    while (running)
    {
        // Check for ESC key
        if (_kbhit())
        {
            int ch = _getch();
            if (ch == 27) running = false;
        }
        
        // Check for window close button
        MSG msg;
        if (PeekMessage(&msg, GetHWnd(), 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_CLOSE || msg.message == WM_DESTROY)
            {
                running = false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        cleardevice();

        if (state == AnimationState::Opening)
        {
            // Opening animation: particles gather with spiral rotation
            double progress = animTime / OPENING_DURATION;
            
            for (int i = 0; i < NUM_POINTS; i++)
            {
                // Calculate individual progress with delay
                double individualProgress = progress - particles[i].delay;
                if (individualProgress < 0) individualProgress = 0;
                if (individualProgress > 1) individualProgress = 1;
                
                // Apply easing for smooth deceleration
                double easedProgress = easeOutCubic(individualProgress);
                
                // Rotation decreases as particles approach target
                particles[i].rotAngle += particles[i].rotSpeed * (1.0 - easedProgress * 0.8);
                
                // Apply spiral rotation to start position
                double cosA = cos(particles[i].rotAngle);
                double sinA = sin(particles[i].rotAngle);
                
                Point3D rotatedStart;
                rotatedStart.x = particles[i].start.x * cosA - particles[i].start.z * sinA;
                rotatedStart.y = particles[i].start.y;
                rotatedStart.z = particles[i].start.x * sinA + particles[i].start.z * cosA;
                
                // Interpolate from rotated start to target
                particles[i].current.x = rotatedStart.x + (particles[i].target.x - rotatedStart.x) * easedProgress;
                particles[i].current.y = rotatedStart.y + (particles[i].target.y - rotatedStart.y) * easedProgress;
                particles[i].current.z = rotatedStart.z + (particles[i].target.z - rotatedStart.z) * easedProgress;
                
                // Add turbulence during flight (decreases as particles approach)
                double turbulenceStrength = (1.0 - easedProgress) * 8.0;
                particles[i].current.x += sin(animTime * 8 + i * 0.1) * turbulenceStrength;
                particles[i].current.y += cos(animTime * 6 + i * 0.15) * turbulenceStrength;
                particles[i].current.z += sin(animTime * 7 + i * 0.12) * turbulenceStrength * 0.5;
            }
            
            // Transition to heartbeat when animation complete
            if (progress >= 1.0)
            {
                state = AnimationState::Heartbeat;
                // Ensure all particles are exactly at target positions
                for (int i = 0; i < NUM_POINTS; i++)
                {
                    particles[i].current = particles[i].target;
                }
            }
        }
        else
        {
            // Heartbeat animation - scale around center
            double scale = 1.0 + 0.12 * sin(t);
            
            for (int i = 0; i < NUM_POINTS; i++)
            {
                particles[i].current.x = particles[i].target.x * scale;
                particles[i].current.y = particles[i].target.y * scale;
                particles[i].current.z = particles[i].target.z * scale;
            }
            
            t += 0.1;
        }

        // Render particles
        for (int i = 0; i < NUM_POINTS; i++)
        {
            double x = particles[i].current.x;
            double y = particles[i].current.y;
            double z = particles[i].current.z;

            // Perspective projection
            double perspective = 500.0 / (500.0 + z);

            renderPoints[i].px = CENTER_X + (int)(x * perspective);
            renderPoints[i].py = CENTER_Y - (int)(y * perspective);
            renderPoints[i].depth = z;
            renderPoints[i].posX = x;
            renderPoints[i].posY = y;
            
            // Brightness based on depth (normalized for heart's z range)
            // Creates gradient from deep purple to lavender
            double depthNorm = (z + 80) / 160.0;
            depthNorm = depthNorm < 0 ? 0 : (depthNorm > 1 ? 1 : depthNorm);
            renderPoints[i].brightness = (int)(160 + depthNorm * 70);
        }

        // Depth sort
        std::sort(renderPoints.get(), renderPoints.get() + NUM_POINTS, 
            [](const RenderPoint& a, const RenderPoint& b) {
                return a.depth < b.depth;
            });

        // Draw particles with romantic color gradient
        for (int i = 0; i < NUM_POINTS; i++)
        {
            int b = renderPoints[i].brightness;
            
            // Particle size varies during opening animation
            int radius = 2;
            if (state == AnimationState::Opening)
            {
                double progress = animTime / OPENING_DURATION;
                // Particles shrink as they gather
                double sizeProgress = easeInOutCubic((progress * 1.5 < 1.0) ? progress * 1.5 : 1.0);
                radius = (int)(3.5 * (1.0 - sizeProgress) + 2.0 * sizeProgress);
            }
            
            // Color palette: purple-blue, violet, indigo
            // Use depth and position for smooth natural gradient
            double depthNorm = (renderPoints[i].depth + 80) / 160.0;
            depthNorm = depthNorm < 0 ? 0 : (depthNorm > 1 ? 1 : depthNorm);
            
            // Get actual 3D position for color variation
            double posX = renderPoints[i].posX;
            double posY = renderPoints[i].posY;
            double posFactor = sqrt(posX * posX + posY * posY) / 150.0;
            posFactor = posFactor > 1 ? 1 : posFactor;
            
            // Purple-blue gradient: deep purple-blue -> violet -> soft indigo
            // Balanced R and B for purple-blue, low G
            double rFactor = 0.78 - depthNorm * 0.1 + posFactor * 0.05;
            double gFactor = 0.45 + depthNorm * 0.25;
            double bFactor = 0.88 + depthNorm * 0.07 - posFactor * 0.03;
            
            int r = (int)(b * rFactor);
            int g = (int)(b * gFactor);
            int bl = (int)(b * bFactor);
            
            // Add subtle shimmer during heartbeat
            if (state == AnimationState::Heartbeat)
            {
                double shimmer = sin(t * 2 + renderPoints[i].depth * 0.05) * 0.08 + 1.0;
                r = (int)(r * shimmer);
                g = (int)(g * shimmer);
                bl = (int)(bl * shimmer);
            }
            
            // Clamp values
            r = r > 255 ? 255 : (r < 0 ? 0 : r);
            g = g > 255 ? 255 : (g < 0 ? 0 : g);
            bl = bl > 255 ? 255 : (bl < 0 ? 0 : bl);
            
            setfillcolor(RGB(r, g, bl));
            solidcircle(renderPoints[i].px, renderPoints[i].py, radius);
        }

        FlushBatchDraw();
        Sleep(20);
        animTime += 0.02;
    }

    EndBatchDraw();
    closegraph();
    return 0;
}
