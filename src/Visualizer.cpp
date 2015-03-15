#include "Resource.h"
#include "Visualizer.h"

#include <GL/freeglut.h>
#include <GL/glew.h>

#include <cmath>
#include <sstream>
#include <stdlib.h>
#include <sys/time.h>

namespace {
void checkError(int line) {
    GLenum err = glGetError();
    if (err) {
        std::cerr << "***ERROR*** " << line
                  << ": " << gluErrorString(err) << " ***ERROR*" << std::endl;
    }
}
}

#define ERRORCHECK() do {                       \
        checkError(__LINE__);                   \
    } while (0)


Visualizer::Visualizer(const Repeater::Ptr& rep): mRepeater(rep),
                                                  mWidth(0),
                                                  mHeight(0),
                                                  mZoom(1),
                                                  mVolume(0),
                                                  mCurAdjustment(0),
						  mLastAdjustTime(0)
{
    mAdjustments.insert(
        std::make_pair(
            'f', Adjustment(
                "feedback",
                [](Repeater::Knobs& k, double a) -> double {
                    k.mode = Repeater::M_FEEDBACK;
                    double& tt = k.levels[k.mode];
                    tt += a/25;
                    return (tt = std::max(0.0, std::min(1.0, tt)));
                })
            )
        );
    mAdjustments.insert(
        std::make_pair(
            'F', Adjustment(
                "threshold",
                [](Repeater::Knobs& k, double a) -> double {
                    double& tt = k.feedbackThreshold;
                    tt *= 1 + a/10;
                    return (tt = std::max(1e-6, std::min(1.0, tt)));
                })
            )
        );
    mAdjustments.insert(
        std::make_pair(
            'g', Adjustment(
                "gain",
                [](Repeater::Knobs& k, double a) -> double {
                    k.mode = Repeater::M_GAIN;
                    double& tt = k.levels[k.mode];
                    tt += a/25;
                    return (tt = std::max(0.0, tt));
                })
            )
        );
    mAdjustments.insert(
        std::make_pair(
            't', Adjustment(
                "target",
                [](Repeater::Knobs& k, double a) -> double {
                    k.mode = Repeater::M_TARGET;
                    double& tt = k.levels[k.mode];
                    tt += a/25;
                    return (tt = std::max(0.0, std::min(1.0, tt)));
                })
            )
        );
    mAdjustments.insert(
        std::make_pair(
            'l', Adjustment(
                "limit",
                [](Repeater::Knobs& k, double a) -> double {
                    double& tt = k.limitPower;
                    tt += a/100;
                    return (tt = std::max(0.01, std::min(1.0, tt)));
                })
            )
        );
    mAdjustments.insert(
        std::make_pair(
            'd', Adjustment(
                "dampen",
                [](Repeater::Knobs& k, double a) -> double {
                    double& tt = k.dampen;
                    tt = (tt + (a + 1)/2)/2;
                    return (tt = std::max(0.0, std::min(1.0, tt)));
                })
            )
        );
}

void Visualizer::onInit() {
    Shader::Ptr rect = std::make_shared<Shader>(GL_VERTEX_SHADER,
                                                LOAD_RESOURCE(src_rect_vert));
    Shader::Ptr polar = std::make_shared<Shader>(GL_VERTEX_SHADER,
                                                 LOAD_RESOURCE(src_polar_vert));
    Shader::Ptr color = std::make_shared<Shader>(GL_FRAGMENT_SHADER,
                                                 LOAD_RESOURCE(src_color_frag));
    ERRORCHECK();

    mRoundShader = std::make_shared<ShaderProgram>();
    mRoundShader->attach(polar);
    mRoundShader->attach(color);
    ERRORCHECK();

    mSquareShader = std::make_shared<ShaderProgram>();
    mSquareShader->attach(rect);
    mSquareShader->attach(color);
    ERRORCHECK();

    GLint numBufs, numSamples;
    glGetIntegerv(GL_SAMPLE_BUFFERS, &numBufs);
    glGetIntegerv(GL_SAMPLES, &numSamples);
    std::cout << "Multisample configuration: " << numBufs << " buffers, "
              << numSamples << " samples" << std::endl;

    // this is stupid and hacky
    switch (mRepeater->getKnobs().mode) {
    case Repeater::M_GAIN:
        mCurAdjustment = 'g';
        break;
    case Repeater::M_TARGET:
        mCurAdjustment = 't';
        break;
    case Repeater::M_FEEDBACK:
        mCurAdjustment = 'f';
        break;
    }
}

void Visualizer::onResize(int x, int y) {
    mWidth = x;
    mHeight = y;
}

namespace {
struct Point {
    double x, y, z, r, g, b, a;
    Point(): x(0), y(0), z(0), r(0), g(0), b(0), a(0) {}
};
typedef std::vector<Point> Points;

struct Span {
    Point in, out;
};
typedef std::vector<Span> Spans;

class LineLoop: public Points {
public:
    LineLoop(size_t count): Points(count) {
        iterator iter = begin();
        for (size_t i = 0; i < count; i++) {
            iter->x = i*M_PI*2/count;
            ++iter;
        }
    }

    void draw() {
        glVertexPointer(3, GL_DOUBLE, sizeof(Point), &front().x);
        glColorPointer(4, GL_DOUBLE, sizeof(Point), &front().r);
        glDrawArrays(GL_LINE_LOOP, 0, size());
    }
};

class QuadLoop: public Spans {
public:
    QuadLoop(size_t count): Spans(count + 1) {
        iterator iter = begin();

        for (size_t i = 0; i < count; i++) {
            iter->in.x = iter->out.x = i*M_PI*2/count;
            ++iter;
        }
    }

    void draw() {
        back() = front();
        glVertexPointer(3, GL_DOUBLE, sizeof(Point), &front().in.x);
        glColorPointer(4, GL_DOUBLE, sizeof(Point), &front().in.r);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, size()*2);
    }

    void drawOutline() {
        glVertexPointer(3, GL_DOUBLE, sizeof(Span), &front().out.x);
        glColorPointer(4, GL_DOUBLE, sizeof(Span), &front().out.r);
        glDrawArrays(GL_LINE_LOOP, 0, size());
    }
};
}

void Visualizer::drawHistory() {
    glPushMatrix();

/*
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  double time = tv.tv_sec + tv.tv_nsec*1e-9;
  glRotatef(time*360/mRepeater->getOptions().loopDelay/4, 0, 0, -1);
*/

    mRepeater->getHistory(mHistory);

    mRoundShader->bind();
    ERRORCHECK();

    const size_t count = mHistory.history.size();

    double maxR = 1e-6;

    // TODO make these persistent
    QuadLoop power(count);
    LineLoop limit(count);
    QuadLoop expected(count);
    LineLoop playback(count);
    {
        QuadLoop::iterator pi = power.begin();
        QuadLoop::iterator ei = expected.begin();
        LineLoop::iterator li = limit.begin();
        LineLoop::iterator pb = playback.begin();
        for (auto dp : mHistory.history) {
            //maxR = std::max(maxR, li->y = dp.limitPower);
            li->y = dp.limitPower;
            li++;

            maxR = std::max(maxR, ei->out.y = dp.expectedPower);
            ei++;

            maxR = std::max(maxR, pi->out.y = dp.recordedPower);
            pi->in.r = dp.recordedPower/dp.limitPower;
            pi->out.r = dp.recordedPower;
            pi->in.a = 0.1;
            pi->out.a = 0.8;
            pi++;

            maxR = std::max(maxR, pb->y = dp.expectedPower*dp.actualGain);
            pb++;
        }
    }

    mZoom = mZoom*0.9 + 0.1*0.97/maxR;
    glScalef(mZoom, mZoom, mZoom);

    glEnableClientState(GL_VERTEX_ARRAY);

    glColor4f(0.5, 0.5, 0, 0.3);
    expected.draw();
    glColor4f(1, 1, 0, 1);
    glLineWidth(1);
    expected.drawOutline();

    glEnableClientState(GL_COLOR_ARRAY);
    power.draw();
    glDisableClientState(GL_COLOR_ARRAY);

    glColor4f(1, 0, 0, 1);
    glLineWidth(2);
    limit.draw();

    glColor4f(0, 1, 0, 0.5);
    glLineWidth(0.5);
    playback.draw();

    ERRORCHECK();

    glDisableClientState(GL_VERTEX_ARRAY);

    glLineWidth(2);

    glBegin(GL_LINES);
    glColor4f(0, 1, 0, 0.5);
    glVertex3f(power[mHistory.playPos].in.x, 0, 0);
    glVertex3f(power[mHistory.playPos].in.x, 100, 0);
    glColor4f(0, 0, 1, 0.5);
    glVertex3f(power[mHistory.recordPos].in.x, 0, 0);
    glVertex3f(power[mHistory.recordPos].in.x, 100, 0);
    glEnd();

    {
        size_t i = mHistory.recordPos;
        double x = power[i].in.x;
        const auto& dp = mHistory.history[i];

        mVolume = mVolume*0.95 + dp.expectedPower*0.05;

        glPointSize(10);
        glBegin(GL_POINTS);
        glColor4f(1, 0, 1, 0.5);
        glVertex3f(x, mVolume*dp.targetGain, 0);
        glColor4f(0, 0.5, 0.5, 0.5);
        glVertex3f(x, mVolume*dp.actualGain, 0);
        glEnd();
        glPointSize(7);
        glBegin(GL_POINTS);
        glColor4f(0, 0.7, 0.7, 0.7);
        glVertex3f(x, mVolume*dp.actualGain, 0);
        glEnd();
    }


    ERRORCHECK();

    glPopMatrix();
}

namespace {
double getTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec*1e-9;
}
}

void Visualizer::onKeyboard(unsigned char c) {
    mLastAdjustTime = getTime();
    mCurAdjustment = c;
    auto adj = mAdjustments.find(c);
    if (adj != mAdjustments.end()) {
        Repeater::Knobs k = mRepeater->getKnobs();
        adj->second.cb(k, 0);
        mRepeater->setKnobs(k);
        std::cout << "set mode to " << adj->second.name << std::endl;
    }
}

void Visualizer::onSpecialKey(int c) {
    mLastAdjustTime = getTime();
    double adjust = 0;
    std::cout << "specialKey " << c << std::endl;
    switch (c) {
    case GLUT_KEY_UP:
        adjust = 1;
        break;
    case GLUT_KEY_DOWN:
        adjust = -1;
        break;
    }

    auto adj = mAdjustments.find(mCurAdjustment);
    if (adj != mAdjustments.end()) {
        Repeater::Knobs k = mRepeater->getKnobs();
        double r = adj->second.cb(k, adjust);
        mRepeater->setKnobs(k);
        std::cout << "adjusted " << adj->second.name << " to " << r << std::endl;
    }
}

void Visualizer::drawBanner() {
    mSquareShader->bind();

    {
        std::stringstream message;

        if (mCurAdjustment && getTime() - mLastAdjustTime < 3) {
            auto adj = mAdjustments.find(mCurAdjustment);
            if (adj != mAdjustments.end()) {
                glColor4f(0,0,0.5,1);
                Repeater::Knobs k = mRepeater->getKnobs();
                message << adj->second.name << ": " << adj->second.cb(k, 0.0f);
            } else {
                glColor4f(0.5,0,0,1);
                for (auto list : mAdjustments) {
                    message << list.first << ':' << list.second.name << ' ';
                }
            }
        } else {
            switch (mRepeater->getState()) {
            case Repeater::S_STARTUP:
                glColor4f(1, 0, 0, 1);
                message << "acquiring signal";
                break;
            case Repeater::S_RUNNING:
                glColor4f(0, 0, 0, 0.7);
                message << "whatwesaidwillbe";
                break;
            case Repeater::S_SHUTDOWN_REQUESTED:
            case Repeater::S_SHUTTING_DOWN:
            case Repeater::S_GONE:
                glColor4f(0, 0, 1, 1);
                message << "whatwesaidonlywas";
                break;
            }
        }

        void *font = GLUT_BITMAP_HELVETICA_18;
        glRasterPos2f(-mWidth*1.0/mHeight, -1.0 + 9.0/mHeight);
        glutBitmapString(font, (const unsigned char *)message.str().c_str());
    }

    if (0) {
        std::stringstream message;
        const Repeater::Knobs& k = mRepeater->getKnobs();
        message << "mode: ";
        switch (k.mode) {
        case Repeater::M_GAIN:
            message << "gain";
            break;
        case Repeater::M_TARGET:
            message << "target";
            break;
        case Repeater::M_FEEDBACK:
            message << "feedback";
            break;
        }
        message << " " << k.levels.find(k.mode)->second;
        size_t width = glutBitmapLength(GLUT_BITMAP_HELVETICA_18,
                                        (const unsigned char *)message.str().c_str());
        glRasterPos2f((mWidth - 2*width)*1.0/mHeight, -1);

        glColor4f(0, 0, 0, 1);
        glutBitmapString(GLUT_BITMAP_HELVETICA_18,
                         (const unsigned char *)message.str().c_str());
    }
}

bool Visualizer::onDisplay() {
    glViewport(0, 0, mWidth, mHeight);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_POLYGON_SMOOTH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-mWidth*1.0/mHeight, mWidth*1.0/mHeight, -1, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (mRepeater->getState() != Repeater::S_STARTUP) {
        drawHistory();
    }

    drawBanner();

    glutSwapBuffers();

    return mRepeater->getState() == Repeater::S_GONE;
}
