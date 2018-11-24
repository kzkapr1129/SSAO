#include <math.h>
#include <stdio.h>
#include <string.h>
#include <GL/glew.h>
#include <GL/glut.h>

#define WINDOW_WIDTH 364
#define WINDOW_HEIGHT 364
#define FBO_WIDTH 364
#define FBO_HEIGHT 364
#define NUM_LIGHT 1
#define WINDOW_ASPECT ((float)WINDOW_WIDTH/(float)WINDOW_HEIGHT)
#define SCREEN_FAR 100.0
#define SCREEN_NEAR 0.1
#define NUM_SAMPLE_POINTS 8
#define MAX_ENV 0.13
#define CAM_POSX 0.0
#define CAM_POSY 0.0
#define CAM_POSZ 5.5

/**
 * ATI constant values. please refer link as well.
 * https://www.opengl.org/registry/specs/ATI/texture_float.txt
 */
#define RGBA_FLOAT32_ATI                 0x8814
#define RGB_FLOAT32_ATI                  0x8815
#define ALPHA_FLOAT32_ATI                0x8816
#define INTENSITY_FLOAT32_ATI            0x8817
#define LUMINANCE_FLOAT32_ATI            0x8818
#define LUMINANCE_ALPHA_FLOAT32_ATI      0x8819
#define RGBA_FLOAT16_ATI                 0x881A
#define RGB_FLOAT16_ATI                  0x881B
#define ALPHA_FLOAT16_ATI                0x881C
#define INTENSITY_FLOAT16_ATI            0x881D
#define LUMINANCE_FLOAT16_ATI            0x881E
#define LUMINANCE_ALPHA_FLOAT16_ATI      0x881F

const float SAMPLE_POINTS[NUM_SAMPLE_POINTS*2] = {
    3, 3,
    1, 1,
    0, 3,
    0, 1,
    3, 0,
    1, 0,
    3,-3,
    1,-1,
};

const GLchar* PASS1_VERT_SHADER =
    "#version 130\n"
    "uniform mat4 in_MVP;\n"
    "uniform mat4 in_MV;\n"
    "uniform mat4 in_Normal_MV;\n" // ModelView行列から平行移動を除いた回転のみの行列
    "attribute vec4 in_Position;\n"
    "attribute vec4 in_Normal;\n"
    "attribute vec2 in_Texture_coord;\n"
    "varying vec4 v_Position;\n"
    "varying vec4 v_Normal;\n"
    "varying vec2 v_texture_coord;\n"
    "void main(void)\n"
    "{\n"
    "    gl_Position = in_MVP * in_Position;\n"
    "    v_Position = in_MV * in_Position;\n"
    "    v_Normal = in_Normal_MV * in_Normal;\n"
    "    v_texture_coord = in_Texture_coord;\n"
    "}\n";

const GLchar* PASS1_FRAG_SHADER =
    "#version 130\n"
    "precision highp float;\n"
    "uniform sampler2D in_Img;\n"
    "varying vec4 v_Position;\n"
    "varying vec4 v_Normal;\n"
    "varying vec2 v_texture_coord;\n"
    "void main(void)\n"
    "{\n"
    "    gl_FragData[0] = v_Position;\n"
    "    gl_FragData[1] = normalize(v_Normal);\n"
    "    gl_FragData[2] = texture2D(in_Img, v_texture_coord);\n"
    "}\n";

const GLchar* PASS2_VERT_SHADER =
    "#version 130\n"
    "attribute vec2 in_Position;\n"
    "attribute vec2 in_Texture_coord;\n"
    "varying vec2 v_texture_coord;\n"
    "void main(void)\n"
    "{\n"
    "    gl_Position = vec4(in_Position, 0.0, 1.0);\n"
    "    v_texture_coord = in_Texture_coord;\n"
    "}\n";

#define STR(str) DOSTR(str)
#define DOSTR(str) # str
const GLchar* PASS2_FRAG_SHADER =
    "#version 130\n"
    "precision highp float;\n"
    "const vec2 fragment_size = vec2(1.0/" STR(FBO_WIDTH) ".0, 1.0/" STR(FBO_HEIGHT) ".0);\n"
    "const vec3 cam_pos = vec3(" STR(CAM_POSX) "," STR(CAM_POSY) "," STR(CAM_POSZ) ");\n"
    "uniform sampler2D in_Position_Img;\n"
    "uniform sampler2D in_Normal_Img;\n"
    "uniform sampler2D in_Albedo_Img;\n"
    "uniform vec3 in_light_pos[" STR(NUM_LIGHT) "];\n" // ライトの座標
    "uniform vec3 in_light_power[" STR(NUM_LIGHT) "];\n" // ライトの出力
    "uniform float in_light_dist[" STR(NUM_LIGHT) "];\n" // スポットライトの減衰開始距離
    "uniform vec2 in_sample_points[" STR(NUM_SAMPLE_POINTS) "];\n"
    "varying vec2 v_texture_coord;\n"

    "float ssao(vec4 pos, vec3 normal)\n"
    "{\n"
#if 1
         // Enable SSAO
    "    float base_dist = length(pos.xyz - cam_pos);\n"
    "    int non_blind_corner = " STR(NUM_SAMPLE_POINTS) ";\n"
    "    for (int i = 0; i < " STR(NUM_SAMPLE_POINTS) "; i++) {\n"
    "        vec2 p1_tex = v_texture_coord + in_sample_points[i]*fragment_size;\n"
    "        vec2 p2_tex = v_texture_coord + (-in_sample_points[i])*fragment_size;\n"
    "        vec3 p1_pos = texture2D(in_Position_Img, p1_tex).xyz;\n"
    "        vec3 p2_pos = texture2D(in_Position_Img, p2_tex).xyz;\n"
    "        float p1_dist = length(p1_pos - cam_pos);\n"
    "        float p2_dist = length(p2_pos - cam_pos);\n"
    "        if (base_dist > p1_dist && base_dist > p2_dist) {\n"
    "            non_blind_corner--;\n"
    "        }\n"
    "    }\n"
    "    return (float(non_blind_corner) / float(" STR(NUM_SAMPLE_POINTS) ")) * " STR(MAX_ENV) ";\n"
#else
         // Disable SSAO
    "    return "STR(MAX_ENV)";"
#endif
    "}\n"

    "void main(void)\n"
    "{\n"
    "    vec4 pos4 = texture2D(in_Position_Img, v_texture_coord);\n"
    "    if (pos4.w <= 0.0) discard;\n" // glClearで塗りつぶされただけの場所は描画しない
    "    vec3 normal = normalize(texture2D(in_Normal_Img, v_texture_coord).xyz);\n"
    "    float ssao_rate = ssao(pos4, normal);\n"
    "    vec3 albedo = texture2D(in_Albedo_Img, v_texture_coord).xyz;\n"
    "    vec3 frag_color = albedo * ssao_rate;\n" // 環境光の計算
#if 1
         // Enable Direct Lighting
    "    for (int i = 0; i < " STR(NUM_LIGHT) "; i++) {\n"
    "        vec3 dist = (in_light_pos[i] - pos4.xyz);\n"
    "        vec3 dir = normalize(dist);\n"
    "        float dir_power = min(1.0, max(0.0, dot(dir, normal)));\n" // 角度に対する光の減衰率
    "        float len_power = 1.0 / pow(max(1.0, length(dist) / in_light_dist[i]),2.0);\n" // 距離に対する光の減衰率(逆2乗)
    "        frag_color += (albedo*in_light_power[i])*(dir_power*len_power);\n" // 拡散反射のみ計算
    "    }\n"
#endif
    "    gl_FragColor = vec4(frag_color, 1.0);\n"
    "}\n";

static GLuint gPass1Program;
static GLuint gPass2Program;
static GLuint gPositionTexture;
static GLuint gNormalTexture;
static GLuint gAlbedoTexture; // "albedo" means texture color
static GLuint gImg;
static GLuint gFrameBufferObject;

struct Lights {
    float pos[3*NUM_LIGHT];
    float power[3*NUM_LIGHT];
    float dist[NUM_LIGHT];
};

static Lights gLights;

static void multiplyMatrix(float* out, const float* src1, const float* src2);
static void getPerspectiveMatrix(float* proj, float aspect,
        int fovy, float near, float far);
static void getModelviewMatrix(float* modelview, float* normalview,
        float ex, float ey, float ez,
        float ax, float ay, float az,
        float ux, float uy, float uz);


void drawBox(float width, float height, float depth) {
    const float box_vertexes[] = {
        // front plane
         width/2.f, height/2.f, depth/2.f,
        -width/2.f, height/2.f, depth/2.f,
         width/2.f,-height/2.f, depth/2.f,
        -width/2.f,-height/2.f, depth/2.f,

        // back plane
        -width/2.f, height/2.f,-depth/2.f,
         width/2.f, height/2.f,-depth/2.f,
        -width/2.f,-height/2.f,-depth/2.f,
         width/2.f,-height/2.f,-depth/2.f,

        // left plane
        -width/2.f, height/2.f, depth/2.f,
        -width/2.f, height/2.f,-depth/2.f,
        -width/2.f,-height/2.f, depth/2.f,
        -width/2.f,-height/2.f,-depth/2.f,

        // right plane
         width/2.f, height/2.f,-depth/2.f,
         width/2.f, height/2.f, depth/2.f,
         width/2.f,-height/2.f,-depth/2.f,
         width/2.f,-height/2.f, depth/2.f,

        // top plane
         width/2.f, height/2.f,-depth/2.f,
        -width/2.f, height/2.f,-depth/2.f,
         width/2.f, height/2.f, depth/2.f,
        -width/2.f, height/2.f, depth/2.f,

        // bottom plane
        -width/2.f,-height/2.f,-depth/2.f,
         width/2.f,-height/2.f,-depth/2.f,
        -width/2.f,-height/2.f, depth/2.f,
         width/2.f,-height/2.f, depth/2.f,
    };

    const float box_normals[] = {
        // front plane
         0, 0, 1,
         0, 0, 1,
         0, 0, 1,
         0, 0, 1,

        // back plane
         0, 0,-1,
         0, 0,-1,
         0, 0,-1,
         0, 0,-1,

        // left plane
        -1, 0, 0,
        -1, 0, 0,
        -1, 0, 0,
        -1, 0, 0,

        // right plane
         1, 0, 0,
         1, 0, 0,
         1, 0, 0,
         1, 0, 0,

        // top plane
         0, 1, 0,
         0, 1, 0,
         0, 1, 0,
         0, 1, 0,

        // bottom plane
         0,-1, 0,
         0,-1, 0,
         0,-1, 0,
         0,-1, 0,
    };

    const float box_texcoords[] = {
             0,  1,
             1,  1,
             0,  0,
             1,  0,

             0,  1,
             1,  1,
             0,  0,
             1,  0,

             0,  1,
             1,  1,
             0,  0,
             1,  0,

             0,  1,
             1,  1,
             0,  0,
             1,  0,

             0,  1,
             1,  1,
             0,  0,
             1,  0,

             0,  1,
             1,  1,
             0,  0,
             1,  0
        };

    const uint16_t box_indexes[] = {
             0, 1, 2,
             2, 1, 3,

             4, 5, 6,
             6, 5, 7,

             8, 9,10,
            10, 9,11,

            12,13,14,
            14,13,15,

            16,17,18,
            18,17,19,

            20,21,22,
            22,21,23
        };

    glVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof (GLfloat) * 3, box_vertexes);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, sizeof (GLfloat) * 3, box_normals);
    glVertexAttribPointer(2, 2, GL_FLOAT, 0, sizeof (GLfloat) * 2, box_texcoords);

    glDrawElements(GL_TRIANGLES, sizeof(box_indexes)/sizeof(uint16_t),
            GL_UNSIGNED_SHORT, box_indexes);
}

const int PRIMES[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43,
    47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103,
    107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163,
    167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227,
    229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
    283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353,
    359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421,
    431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487,
    491, 499, 503, 509, 521, 523, 541, 547, 557, 563, 569,
    571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631,
    641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701,
    709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773,
    787, 797, 809, 811, 821, 823, 827, 829, 839, 853, 857,
    859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937,
    941, 947, 953, 967, 971, 977, 983, 991, 997
};

const int SIZE_OF_PRIMES = sizeof(PRIMES) / sizeof(int);

float halton(int base, int index) {
    const int p = PRIMES[base % SIZE_OF_PRIMES];
    const float half = 1.f / (float)p;
    float sum = 0;
    float k = half;
    while (index > 0) {
        int val = (index % p);
        sum += val * k;
        index = (index - val) / p;
        k *= half;
    }
    return sum;
}

static int gRandIndex = 0;
static void getRandamRoteMat(float* mat) {
    float theta= M_PI * halton(0, gRandIndex);
    float phy  = 2.f * M_PI*halton(1, gRandIndex++);
    float sPhy = sin(theta);
    float at_vec[] = {sPhy*cos(phy), cos(theta), sPhy*sin(phy)};
    float n = sqrt(at_vec[0]*at_vec[0] + at_vec[1]*at_vec[1] + at_vec[2]*at_vec[2]);
    at_vec[0] /= n;
    at_vec[1] /= n;
    at_vec[2] /= n;

    float up[] = {0, 1, 0};
    if ((at_vec[0]*up[0]+at_vec[1]*up[1]+at_vec[2]*up[2]) >= 0.98) {
        up[0] = 0;
        up[1] = 0;
        up[2] = 1;
    }

    float vec_x[] = {
            at_vec[1] * up[2] - up[1] * at_vec[2],
            at_vec[2] * up[0] - up[2] * at_vec[0],
            at_vec[0] * up[1] - up[0] * at_vec[1],
        };

    n = sqrt(vec_x[0]*vec_x[0] + vec_x[1]*vec_x[1] + vec_x[2]*vec_x[2]);
    vec_x[0] /= n;
    vec_x[1] /= n;
    vec_x[2] /= n;

    float vec_y[] = {
            vec_x[1] * at_vec[2] - at_vec[1] * vec_x[2],
            vec_x[2] * at_vec[0] - at_vec[2] * vec_x[0],
            vec_x[0] * at_vec[1] - at_vec[0] * vec_x[1],
        };
    n = sqrt(vec_y[0]*vec_y[0] + vec_y[1]*vec_y[1] + vec_y[2]*vec_y[2]);
    vec_y[0] /= n;
    vec_y[1] /= n;
    vec_y[2] /= n;

    mat[0] = vec_x[0];
    mat[1] = vec_y[0];
    mat[2] = at_vec[0];
    mat[3] = 0;

    mat[4] = vec_x[1];
    mat[5] = vec_y[1];
    mat[6] = at_vec[1];
    mat[7] = 0;

    mat[8] = vec_x[2];
    mat[9] = vec_y[2];
    mat[10] = at_vec[2];
    mat[11] = 0;

    mat[12] = 0;
    mat[13] = 0;
    mat[14] = 0;
    mat[15] = 1;
}

/**
 * geometory to texture
 */
static void draw_pass1() {
    glUseProgram(gPass1Program);

    glViewport(0,0,FBO_WIDTH,FBO_HEIGHT);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, gFrameBufferObject);

    float mvp[16];
    float proj[16];

    float camera_world[16];
    float camera_world_nr[16];
    
    float obj_rote[16];
    
    float modelview[16];
    float modelview_nr[16];

    getPerspectiveMatrix(proj, 1.f, 60, SCREEN_NEAR, SCREEN_FAR);
    getModelviewMatrix(camera_world, camera_world_nr, CAM_POSX, CAM_POSY, CAM_POSZ, 0,0,0, 0,1,0);

    static const GLenum bufs[] = {
      GL_COLOR_ATTACHMENT0_EXT,
      GL_COLOR_ATTACHMENT1_EXT,
      GL_COLOR_ATTACHMENT2_EXT,
    };
    glDrawBuffers(3, bufs);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0,0,0,0); // シェーダで背景画像とポリゴンを識別するため、意図的にalpha値を0にしておく
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(gPass1Program, "in_Img"), 0);
    glBindTexture(GL_TEXTURE_2D, gImg);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    for (int i=0; i < 32; i++) {
        getRandamRoteMat(obj_rote);
        multiplyMatrix(modelview, obj_rote, camera_world);
        multiplyMatrix(modelview_nr, obj_rote, camera_world_nr);
        multiplyMatrix(mvp, modelview, proj);
        glUniformMatrix4fv(glGetUniformLocation(gPass1Program, "in_MVP"), 1, GL_FALSE, mvp);
        glUniformMatrix4fv(glGetUniformLocation(gPass1Program, "in_MV"), 1, GL_FALSE, modelview);
        glUniformMatrix4fv(glGetUniformLocation(gPass1Program, "in_Normal_MV"), 1, GL_FALSE, modelview_nr);

        drawBox(2.5,2.5,2.5);
    }

    glFlush();

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glUseProgram(0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glDrawBuffer(GL_FRONT);

    int err = glGetError();
    if (GL_NO_ERROR != err) {
        printf("Check GL Error in pass1: %d\n", err);
    }
}

/**
 * extract geometory from texture. and render using it.
 */
static void draw_pass2() {
    glUseProgram(gPass2Program);
    glViewport(0,0,WINDOW_WIDTH,WINDOW_HEIGHT);

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    glUniform1i(glGetUniformLocation(gPass2Program, "in_Position_Img"), 0);
    glUniform1i(glGetUniformLocation(gPass2Program, "in_Normal_Img"), 1);
    glUniform1i(glGetUniformLocation(gPass2Program, "in_Albedo_Img"), 2);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPositionTexture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormalTexture);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gAlbedoTexture);

    glUniform3fv(glGetUniformLocation(gPass2Program, "in_light_pos"),
        NUM_LIGHT, gLights.pos);
    glUniform3fv(glGetUniformLocation(gPass2Program, "in_light_power"),
        NUM_LIGHT, gLights.power);
    glUniform1fv(glGetUniformLocation(gPass2Program, "in_light_dist"),
        NUM_LIGHT, gLights.dist);
    glUniform2fv(glGetUniformLocation(gPass2Program, "in_sample_points"),
        NUM_SAMPLE_POINTS, SAMPLE_POINTS);

    const float vertexPointer[] = {
            -1.0,  1.0,
             1.0,  1.0,
            -1.0, -1.0,
             1.0, -1.0
        };

    const float texturePointer[] = {
            0.0,  1.0,
            1.0,  1.0,
            0.0,  0.0,
            1.0,  0.0
        };

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(0, 2, GL_FLOAT, 0, sizeof (GLfloat) * 2, vertexPointer);
    glVertexAttribPointer(2, 2, GL_FLOAT, 0, sizeof (GLfloat) * 2, texturePointer);

    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    glFlush();

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(2);

    int err = glGetError();
    if (GL_NO_ERROR != err) {
        printf("Check GL Error in pass2: %d\n", err);
    }
}

static float angle = 0;
static bool once_pass1 = false;
static void display(void) {
    angle += 0.1f;
    const float radius = 2;
    gLights.pos[0] = radius * cos((((int)angle)%360)*M_PI/180.f);

    if (!once_pass1) {
        draw_pass1();
        once_pass1 = true;
    }

    draw_pass2();

    glFlush();

    int err = glGetError();
    if (GL_NO_ERROR != err) {
        printf("Check GL Error in display(): %d\n", err);
    }
}

static void idle(void) {
    glutPostRedisplay();
}

static void printShaderInfoLog(GLuint shader) {
    GLsizei bufSize;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH , &bufSize);
    if (bufSize > 1) {
        GLchar *infoLog;
        infoLog = (GLchar *)malloc(bufSize);
        if (infoLog != NULL) {
            GLsizei length;
            glGetShaderInfoLog(shader, bufSize, &length, infoLog);
            fprintf(stderr, "InfoLog:\n%s\n\n", infoLog);
            free(infoLog);
        } else {
            fprintf(stderr, "Could not allocate InfoLog buffer.\n");
        }
    }
}

static void printProgramInfoLog(GLuint program) {
    GLsizei bufSize;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH , &bufSize);
    if (bufSize > 1) {
        GLchar *infoLog = (GLchar *)malloc(bufSize);
        if (infoLog != NULL) {
            GLsizei length;
            glGetProgramInfoLog(program, bufSize, &length, infoLog);
            fprintf(stderr, "InfoLog:\n%s\n\n", infoLog);
            free(infoLog);
        } else {
            fprintf(stderr, "Could not allocate InfoLog buffer.\n");
        }
    }
}

static GLuint loadShader(const GLchar* vertSource, const GLchar* fragSource, int pass) {
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

    int length = strlen(vertSource)+1;
    glShaderSource(vert, 1, &vertSource, &length);

    length = strlen(fragSource)+1;
    glShaderSource(frag, 1, &fragSource, &length);

    GLint compiled, linked;

    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        printShaderInfoLog(vert);
        exit(1);
    }

    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        printShaderInfoLog(frag);
        exit(1);
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);

    glDeleteShader(vert);
    glDeleteShader(frag);

    if (pass == 1) {
        glBindAttribLocation(program, 0, "in_Position");
        glBindAttribLocation(program, 1, "in_Normal");
        glBindAttribLocation(program, 2, "in_Texture_coord");
    } else if (pass == 2) {
        glBindAttribLocation(program, 0, "in_Position");
        glBindAttribLocation(program, 2, "in_Texture_coord");
    }

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        printProgramInfoLog(program);
        exit(1);
    }

    return program;
}

static void multiplyMatrix(float* out, const float* src1, const float* src2) {
    for (int i=0; i<16; i++) {
        out[i] = 0;
        const int src1_base = (i/4)*4; // 0->4->8->12
        const int src2_base = i%4;
        for (int j=0; j<4; j++) {
            const int src1_index = src1_base + j;
            const int src2_index = src2_base + j*4;
            out[i] += (src1[src1_index]*src2[src2_index]);
        }
    }
}

static void getPerspectiveMatrix(float* proj, float aspect,
        int fovy, float near, float far) {
    const float f = 1.0f / (float) tan(fovy * (M_PI / 360.0f));
    const float rangeReciprocal = 1.0f / (near - far);

    proj[0] = f / aspect;
    proj[1] = 0.0f;
    proj[2] = 0.0f;
    proj[3] = 0.0f;

    proj[4] = 0.0f;
    proj[5] = f;
    proj[6] = 0.0f;
    proj[7] = 0.0f;

    proj[8] = 0.0f;
    proj[9] = 0.0f;
    proj[10] = (far + near) * rangeReciprocal;
    proj[11] = -1.0f;

    proj[12] = 0.0f;
    proj[13] = 0.0f;
    proj[14] = 2.0f * far * near * rangeReciprocal;
    proj[15] = 0.0f;
}

static void getModelviewMatrix(float* modelview, float* normalview,
        float ex, float ey, float ez,
        float ax, float ay, float az,
        float ux, float uy, float uz) {
    float fx = ax - ex;
    float fy = ay - ey;
    float fz = az - ez;

    // Normalize f
    float rlf = 1.0f / sqrtf(fx * fx + fy * fy + fz * fz);
    fx *= rlf;
    fy *= rlf;
    fz *= rlf;

    // compute s = f x_ up (x_ means "cross product")
    float sx = fy * uz - fz * uy;
    float sy = fz * ux - fx * uz;
    float sz = fx * uy - fy * ux;

    // and normalize s
    float rls = 1.0f / sqrtf(sx * sx + sy * sy + sz * sz);
    sx *= rls;
    sy *= rls;
    sz *= rls;

    // compute u = s x_ f
    float upx = sy * fz - sz * fy;
    float upy = sz * fx - sx * fz;
    float upz = sx * fy - sy * fx;

    const float rote[16] = {
            sx, upx, -fx, 0,
            sy, upy, -fy, 0,
            sz, upz, -fz, 0,
             0,   0,   0, 1
        };
    if (normalview) memcpy(normalview, rote, sizeof(rote));

    const float move[16] = {
             1,   0,   0, 0,
             0,   1,   0, 0,
             0,   0,   1, 0,
           -ex, -ey, -ez, 1
        };

    multiplyMatrix(modelview, rote, move);
}

static void initPass1Shader() {
    glUseProgram(gPass1Program);

    // Positionテクスチャの用意
    glGenTextures(1, &gPositionTexture);
    glBindTexture(GL_TEXTURE_2D, gPositionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, RGBA_FLOAT32_ATI, FBO_WIDTH, FBO_HEIGHT, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Normalテクスチャの用意
    glGenTextures(1, &gNormalTexture);
    glBindTexture(GL_TEXTURE_2D, gNormalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, RGBA_FLOAT32_ATI, FBO_WIDTH, FBO_HEIGHT, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Albedoテクスチャの用意
    glGenTextures(1, &gAlbedoTexture);
    glBindTexture(GL_TEXTURE_2D, gAlbedoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FBO_WIDTH, FBO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // テクスチャマッピングのテクスチャを用意(単純画像のため1画素のみ)
    const uint8_t img[] = {255, 255, 255, 255};
    glGenTextures(1, &gImg);
    glBindTexture(GL_TEXTURE_2D, gImg);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // テクスチャをフレームバッファに関連付ける
    glGenFramebuffersEXT(1, &gFrameBufferObject);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, gFrameBufferObject);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, gPositionTexture, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, gNormalTexture, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, gAlbedoTexture, 0);

    // デプスバッファの用意
    GLuint depthrenderbuffer;
    glGenRenderbuffersEXT(1, &depthrenderbuffer);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthrenderbuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, FBO_WIDTH, FBO_HEIGHT);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depthrenderbuffer);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Failed to initialize FBO\n");
        exit(1);
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    // TODO ライトの初期化
    gLights.pos[0] = 0.0;
    gLights.pos[1] = 0.0;
    gLights.pos[2] = 1.0;

    gLights.power[0] = 1.f;
    gLights.power[1] = 1.f;
    gLights.power[2] = 1.f;

    gLights.dist[0] = 3.5;
}

int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow(argv[0]);
    glutDisplayFunc(display);
    glutIdleFunc(idle);

    int err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Failed to initialize glew: %d\n", err);
        exit(1);
    }

    gPass1Program = loadShader(PASS1_VERT_SHADER, PASS1_FRAG_SHADER, 1);
    initPass1Shader();

    gPass2Program = loadShader(PASS2_VERT_SHADER, PASS2_FRAG_SHADER, 2);

    glutMainLoop();
    return 0;
}

