/*
 * Copyright (C) 2020 Ola Benderius
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <vector>
#include <iostream>
#include <string>
#include <unordered_map>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <GL/glew.h>
#include <GL/glx.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/hash.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "json.hpp"

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, 
    GLXContext, Bool, int32_t const *);

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  Vertex(): pos(), color(), texCoord() {}

  Vertex(glm::vec3 a_pos, glm::vec2 a_texCoord, glm::vec3 a_color): 
    pos(a_pos),
    color(a_color), 
    texCoord(a_texCoord) {}

  bool operator==(Vertex const &a) const {
    return (pos == a.pos && color == a.color && texCoord == a.texCoord);
  }
};

namespace std {
  template<> struct hash<Vertex> {
    size_t operator()(Vertex const &a) const {
      return ((hash<glm::vec3>()(a.pos) ^ (hash<glm::vec3>()(a.color) << 1)) >> 1)
        ^ (hash<glm::vec2>()(a.texCoord) << 1);
    }
  };
}

struct BlockInfo {
  std::string name;
  std::string textureFilename;
  glm::vec3 color;
  glm::vec3 dimension;
  glm::vec2 textureSize;

  BlockInfo(std::string const &a_name, glm::vec3 a_dimension,
      std::string const &a_textureFilename, glm::vec2 a_textureSize):
    name(a_name),
    textureFilename(a_textureFilename),
    color(),
    dimension(a_dimension),
    textureSize(a_textureSize) {}
  
  BlockInfo(std::string const &a_name, glm::vec3 a_dimension,
      glm::vec3 a_color):
    name(a_name),
    textureFilename(),
    color(a_color),
    dimension(a_dimension),
    textureSize() {}
};

struct ModelInfo {
  std::string name;
  std::string modelFilename;
  std::string textureFilename;
  glm::vec3 color;

  ModelInfo(std::string const &a_name, std::string const &a_modelFilename,
      std::string const &a_textureFilename):
    name(a_name),
    modelFilename(a_modelFilename),
    textureFilename(a_textureFilename),
    color() {}
  
  ModelInfo(std::string const &a_name, std::string const &a_modelFilename,
      glm::vec3 a_color):
    name(a_name),
    modelFilename(a_modelFilename),
    textureFilename(),
    color(a_color) {}
};

struct MeshHandle {
  GLuint vao;
  GLuint textureId;
  uint32_t indexOffset;
  uint32_t indexCount;

  MeshHandle():
    vao(),
    textureId(),
    indexOffset(),
    indexCount() {}

  MeshHandle(uint32_t a_indexOffset, uint32_t a_indexCount):
    vao(),
    textureId(),
    indexOffset(a_indexOffset),
    indexCount(a_indexCount) {}
};

static bool isExtensionSupported(char const *extList, char const *extension) {
  char const *where = strchr(extension, ' ');
  if (where || *extension == '\0') {
    return false;
  }
  for (char const *start = extList;;) {
    where = strstr(start, extension);
    if (!where) {
      break;
    }
    char const *terminator = where + strlen(extension);
    if (where == start || *(where - 1) == ' ') {
      if (*terminator == ' ' || *terminator == '\0') {
        return true;
      }
    }
    start = terminator;
  }
  return false;
}

static bool ctxErrorOccurred = false;
static int32_t ctxErrorHandler(Display *, XErrorEvent *) {
  ctxErrorOccurred = true;
  return 0;
}

std::map<std::string, MeshHandle> loadModels(std::vector<ModelInfo> modelInfo,
    std::vector<BlockInfo> blockInfo, GLuint const *vbo, bool verbose)
{
  struct Model {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string name;
    std::string textureFilename;

    Model(std::string const &a_name):
      vertices(), 
      indices(), 
      name(a_name),
      textureFilename() {}
  };

  std::vector<Model> models;

  for (auto info : modelInfo) {
    Model model(info.name);
    model.textureFilename = info.textureFilename;

    if (verbose) {
      std::cout << "Loading model '" << info.name << "'" << std::endl;
    }

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, 
          info.modelFilename.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

    for (auto const &shape : shapes) {
      for (auto const &index : shape.mesh.indices) {
        Vertex vertex = {};

        vertex.pos = {
          attrib.vertices[3 * index.vertex_index + 0],
          attrib.vertices[3 * index.vertex_index + 1],
          attrib.vertices[3 * index.vertex_index + 2]
        };

        if (attrib.texcoords.size() > 0) {
          vertex.texCoord = {
            attrib.texcoords[2 * index.texcoord_index + 0],
            1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
          };
        }

        vertex.color = info.color;

        if (uniqueVertices.count(vertex) == 0) {
          uniqueVertices[vertex] = static_cast<uint32_t>(model.vertices.size());
          model.vertices.push_back(vertex);
        }

        model.indices.push_back(uniqueVertices[vertex]);
      }
    }
    models.push_back(model);
  }

  for (auto info : blockInfo) {
    Model model(info.name);
    model.textureFilename = info.textureFilename;

    if (verbose) {
      std::cout << "Generating box '" << info.name << "'" << std::endl;
    }

    float const xh = info.dimension.x / 2.0f;
    float const yh = info.dimension.y / 2.0f;
    float const zh = info.dimension.z / 2.0f;
    float xtw = info.dimension.x / info.textureSize.x;
    float ytw = info.dimension.y / info.textureSize.x;
    float yth = info.dimension.y / info.textureSize.y;
    float zth = info.dimension.z / info.textureSize.y;
    if (info.textureFilename.empty()) {
      xtw = 0.0f;
      ytw = 0.0f;
      yth = 0.0f;
      zth = 0.0f;
    }

    model.vertices.push_back({{-xh, -yh, -zh}, {0.0f, 0.0f}, info.color});
    model.vertices.push_back({{xh, -yh, -zh}, {xtw, 0.0f}, info.color});
    model.vertices.push_back({{xh, yh, -zh}, {xtw, yth}, info.color});
    model.vertices.push_back({{-xh, yh, -zh}, {0, yth}, info.color});

    model.vertices.push_back({{-xh, -yh, zh}, {0.0f, 0.0f}, info.color});
    model.vertices.push_back({{xh, -yh, zh}, {xtw, 0.0f}, info.color});
    model.vertices.push_back({{xh, yh, zh}, {xtw, yth}, info.color});
    model.vertices.push_back({{-xh, yh, zh}, {0, yth}, info.color});
    
    model.vertices.push_back({{-xh, -yh, -zh}, {0.0f, 0.0f}, info.color});
    model.vertices.push_back({{xh, -yh, -zh}, {xtw, 0.0f}, info.color});
    model.vertices.push_back({{xh, -yh, zh}, {xtw, zth}, info.color});
    model.vertices.push_back({{-xh, -yh, zh}, {0, zth}, info.color});
    
    model.vertices.push_back({{-xh, yh, -zh}, {0.0f, 0.0f}, info.color});
    model.vertices.push_back({{xh, yh, -zh}, {xtw, 0.0f}, info.color});
    model.vertices.push_back({{xh, yh, zh}, {xtw, zth}, info.color});
    model.vertices.push_back({{-xh, yh, zh}, {0, zth}, info.color});

    model.vertices.push_back({{-xh, -yh, -zh}, {0.0f, 0.0f}, info.color});
    model.vertices.push_back({{-xh, yh, -zh}, {ytw, 0.0f}, info.color});
    model.vertices.push_back({{-xh, yh, zh}, {ytw, zth}, info.color});
    model.vertices.push_back({{-xh, -yh, zh}, {0, zth}, info.color});
    
    model.vertices.push_back({{xh, -yh, -zh}, {0.0f, 0.0f}, info.color});
    model.vertices.push_back({{xh, yh, -zh}, {ytw, 0.0f}, info.color});
    model.vertices.push_back({{xh, yh, zh}, {ytw, zth}, info.color});
    model.vertices.push_back({{xh, -yh, zh}, {0, zth}, info.color});

    for (uint32_t i{0}; i <= 20; i = i + 4) {
      model.indices.push_back(i);
      model.indices.push_back(i + 1);
      model.indices.push_back(i + 2);
      model.indices.push_back(i);
      model.indices.push_back(i + 2);
      model.indices.push_back(i + 3);
    }

    models.push_back(model);
  }

  uint32_t vertexCount{0};
  uint32_t indexCount{0};
  for (Model model : models) {
    vertexCount += model.vertices.size();
    indexCount += model.indices.size();
  }
  
  {
    uint32_t dataSizeTotal = 8 * vertexCount * sizeof(float);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, dataSizeTotal, 0, GL_STATIC_DRAW);
    {
      uint32_t bufferPos{0};
      for (Model model : models) {
        uint32_t dataSize = 8 * model.vertices.size() * sizeof(float);
        glBufferSubData(GL_ARRAY_BUFFER, bufferPos, dataSize,
            model.vertices.data());
        bufferPos += dataSize;
      }
    }
  }

  std::map<std::string, MeshHandle> handles;
  {
    uint32_t dataSizeTotal = indexCount * sizeof(uint32_t);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, dataSizeTotal, 0, GL_STATIC_DRAW);
    {
      uint32_t bufferPos{0};
      for (Model model : models) {
        uint32_t dataSize = model.indices.size() * sizeof(uint32_t);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, bufferPos, dataSize,
            model.indices.data());
        handles[model.name] = MeshHandle(bufferPos, model.indices.size());
        bufferPos += dataSize;
      }
    }
  }
  
  for (auto model : models) {
    if (!model.textureFilename.empty()) {
      int32_t w;
      int32_t h;
      int32_t c;
      stbi_uc *tex = stbi_load(model.textureFilename.c_str(), &w, &h, &c,
          STBI_rgb_alpha);

      glGenTextures(1, &handles[model.name].textureId);
      glBindTexture(GL_TEXTURE_2D, handles[model.name].textureId);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
          reinterpret_cast<void *>(tex));

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      glBindTexture(GL_TEXTURE_2D, 0);
    
      stbi_image_free(tex);
    }
  }

  {
    uint32_t stride = sizeof(Vertex);
    uint32_t firstVertexOffset{0};
    for (Model model : models) {
      GLuint vao;
      glGenVertexArrays(1, &vao);
      glBindVertexArray(vao);

      glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
      
      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glEnableVertexAttribArray(2);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 
          reinterpret_cast<void *>(firstVertexOffset));
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, 
          reinterpret_cast<void *>(firstVertexOffset + 3 * sizeof(float)));
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, 
          reinterpret_cast<void *>(firstVertexOffset + 6 * sizeof(float)));

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[1]);
      firstVertexOffset += 8 * model.vertices.size() * sizeof(float);
      handles[model.name].vao = vao;
    }
  }

  return handles;
}

GLuint buildShaders(std::string const &vertexShaderGlsl,
    std::string const &fragmentShaderGlsl) {
  GLuint vertShaderId = glCreateShader(GL_VERTEX_SHADER);
  {

    char const *vertSourcePtr = vertexShaderGlsl.c_str();
    glShaderSource(vertShaderId, 1, &vertSourcePtr, nullptr);
    glCompileShader(vertShaderId);

    int32_t infoLogLen;
    GLint res = GL_FALSE;
    glGetShaderiv(vertShaderId, GL_COMPILE_STATUS, &res);
    glGetShaderiv(vertShaderId, GL_INFO_LOG_LENGTH, &infoLogLen);

    if (infoLogLen > 0) {
      std::string errorMsg(infoLogLen, ' ');
      glGetShaderInfoLog(vertShaderId, infoLogLen, nullptr, &errorMsg[0]);
      std::cerr << "Vertex shader error: " << std::endl << errorMsg 
        << std::endl;
    }
  }

  GLuint fragShaderId = glCreateShader(GL_FRAGMENT_SHADER);
  {
    char const *fragSourcePtr = fragmentShaderGlsl.c_str();
    glShaderSource(fragShaderId, 1, &fragSourcePtr, nullptr);
    glCompileShader(fragShaderId);

    int32_t infoLogLen;
    GLint res = GL_FALSE;
    glGetShaderiv(fragShaderId, GL_COMPILE_STATUS, &res);
    glGetShaderiv(fragShaderId, GL_INFO_LOG_LENGTH, &infoLogLen);
    if (infoLogLen > 0) {
      std::string errorMsg(infoLogLen, ' ');
      glGetShaderInfoLog(fragShaderId, infoLogLen, nullptr, &errorMsg[0]);
      std::cerr << "Fragment shader error: " << std::endl << errorMsg
        << std::endl;
    }
  }

  GLuint programId = glCreateProgram();
  {
    glAttachShader(programId, vertShaderId);
    glAttachShader(programId, fragShaderId);
    glLinkProgram(programId);

    int32_t infoLogLen;
    GLint res = GL_FALSE;
    glGetShaderiv(fragShaderId, GL_COMPILE_STATUS, &res);
    glGetShaderiv(fragShaderId, GL_INFO_LOG_LENGTH, &infoLogLen);
    if (infoLogLen > 0) {
      std::string errorMsg(infoLogLen, ' ');
      glGetShaderInfoLog(fragShaderId, infoLogLen, nullptr, &errorMsg[0]);
      std::cerr << "Shader program error: " << std::endl << errorMsg
        << std::endl;
    }

    glDetachShader(programId, vertShaderId);
    glDetachShader(programId, fragShaderId);

    glDeleteShader(vertShaderId);
    glDeleteShader(fragShaderId);
  }

  return programId;
}

int32_t main(int32_t argc, char **argv) {
  int32_t retCode{EXIT_SUCCESS};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if (0 == commandlineArguments.count("cid") 
      || 0 == commandlineArguments.count("map-path") 
      || 0 == commandlineArguments.count("map-path") 
      || 0 == commandlineArguments.count("freq") 
      || 0 == commandlineArguments.count("width") 
      || 0 == commandlineArguments.count("height")
      || 0 == commandlineArguments.count("fovy")) {
    std::cerr << argv[0] << " simulates a camera sensor." << std::endl;
    std::cerr << "Usage:   " << argv[0] << std::endl
      << "  --cid=<OD4 session> " << std::endl
      << "  --map-path=<Folder where the map is stored> " << std::endl
      << "  --freq=<Frequency of camera> " << std::endl
      << "  --width=<Width of the output image> " << std::endl
      << "  --height=<Height of the output image> " << std::endl
      << "  --fovy=<Camera vertical field of view> " << std::endl
      << "  [--frame-id=<The frame to use for the true position, default: 0>] " 
      << std::endl
      << "  [--x=<Mount X position (forward), default: 0.0>] " << std::endl
      << "  [--y=<Mount Y position (left), default: 0.0>] " << std::endl
      << "  [--z=<Mount Z position (up), default: 0.0>] " << std::endl
      << "  [--yaw=<Mount yaw angle, default: 0.0>] " << std::endl
      << "  [--pitch=<Mount pitch angle, default: 0.0>] " << std::endl
      << "  [--roll=<Mount roll angle, default: 0.0>] " << std::endl
      << "  [--name.i420=<Shared memory for I420 data, default: video0.i420>] " 
      << std::endl
      << "  [--name.argb=<Shared memory for ARGB data, default: video0.argb>] "
      << std::endl
      << "  [--verbose]" << std::endl << std::endl
      << "Example: " << argv[0] << " --cid=111 --frame-id=0 "
      << "--map-path=../resource/example_map --x=1.3 --z=0.5 "
      << "--width=1280 --height=720 --fovy=48.8 --freq=5 --verbose" 
      << std::endl;
    retCode = 1;
  } else {
    std::string const mapPath{commandlineArguments["map-path"]};
    std::string const nameI420{(commandlineArguments["name.i420"].size() != 0) 
      ? commandlineArguments["name.i420"] : "video0.i420"};
    std::string const nameArgb{(commandlineArguments["name.argb"].size() != 0) 
      ? commandlineArguments["name.argb"] : "video0.argb"};
    uint32_t const freq = std::stoi(commandlineArguments["freq"]);
    uint32_t const width = std::stoi(commandlineArguments["width"]);
    uint32_t const height = std::stoi(commandlineArguments["height"]);
    float const fovy = std::stof(commandlineArguments["fovy"]);
    uint16_t const cid = std::stoi(commandlineArguments["cid"]);
    uint32_t const frameId = (commandlineArguments["frame-id"].size() != 0)
      ? std::stoi(commandlineArguments["frame-id"]) : 0;
    bool const verbose{commandlineArguments.count("verbose") != 0};

    float const aspect = static_cast<float>(width) / static_cast<float>(height);
    
    glm::vec3 mountPos((commandlineArguments["x"].size() != 0) 
        ? std::stod(commandlineArguments["x"]) : 0.0,
        (commandlineArguments["y"].size() != 0) 
        ? std::stod(commandlineArguments["y"]) : 0.0,
        (commandlineArguments["z"].size() != 0) 
        ? std::stod(commandlineArguments["z"]) : 0.0);
    glm::quat mountRot(glm::vec3(
        (commandlineArguments["roll"].size() != 0) 
        ? std::stod(commandlineArguments["roll"]) : 0.0,
        (commandlineArguments["pitch"].size() != 0) 
        ? std::stod(commandlineArguments["pitch"]) : 0.0,
        (commandlineArguments["yaw"].size() != 0) 
        ? std::stod(commandlineArguments["yaw"]) : 0.0));


    Display *display = XOpenDisplay(nullptr);
    if (!display) {
      std::cerr << "Could not open X display" << std::endl;
      return -1;
    }

    static int32_t visualAttribs[] =
      {
        GLX_X_RENDERABLE    , True,
        GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE     , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
        GLX_RED_SIZE        , 8,
        GLX_GREEN_SIZE      , 8,
        GLX_BLUE_SIZE       , 8,
        GLX_ALPHA_SIZE      , 8,
        GLX_DEPTH_SIZE      , 24,
        GLX_STENCIL_SIZE    , 8,
        GLX_DOUBLEBUFFER    , True,
        //GLX_SAMPLE_BUFFERS  , 1,
        //GLX_SAMPLES         , 4,
        None
      };

    int32_t glxMajor;
    int32_t glxMinor;
   
    if (!glXQueryVersion(display, &glxMajor, &glxMinor) ||
        ((glxMajor == 1 ) && (glxMinor < 3)) || (glxMajor < 1)) {
      std::cerr << "Invalid GLX version" << std::endl;
      return -1;
    }

    int32_t fbcount;
    GLXFBConfig *fbc = glXChooseFBConfig(display, DefaultScreen(display), 
        visualAttribs, &fbcount);
    if (!fbc) {
      std::cerr << "Failed to retrieve a framebuffer config" << std::endl;
      return -1;
    }
    if (verbose) {
      std::clog << "Found " << fbcount << " matching FB configs" << std::endl;
    }

    int32_t bestFbc = -1;
    int32_t worstFbc = -1;
    int32_t bestNumSamp = -1;
    int32_t worstNumSamp = 999;

    for (int32_t i{0}; i < fbcount; ++i) {
      XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[i]);
      if (vi) {
        int32_t sampBuf;
        int32_t samples;
        glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLE_BUFFERS, &sampBuf);
        glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLES, &samples);

        if (verbose) {
          std::clog << "Matching fbconfig " << i << ", visual ID " 
            << vi->visualid << ": SAMPLE_BUFFERS = " << sampBuf << ", SAMPLES = "
            << samples << std::endl;
        }
        
        if (bestFbc < 0 || (sampBuf && samples > bestNumSamp)) {
          bestFbc = i;
          bestNumSamp = samples;
        }
        if (worstFbc < 0 || (!sampBuf || samples < worstNumSamp)) {
          worstFbc = i;
          worstNumSamp = samples;
        }
      }
      XFree(vi);
    }

    GLXFBConfig bestFbConfig = fbc[bestFbc];
    XFree(fbc);

    XVisualInfo *vi = glXGetVisualFromFBConfig(display, bestFbConfig);
    if (verbose) {
      std::clog << "Selected visual ID = " << vi->visualid << std::endl;
    }

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(display, RootWindow(display, vi->screen),
        vi->visual, AllocNone);
    swa.background_pixmap = None;
    swa.border_pixel = 0;
    swa.event_mask = StructureNotifyMask;
    
    Colormap cmap = swa.colormap;;

    Window win = XCreateWindow(display, RootWindow(display, vi->screen), 0, 0,
        width, height, 0, vi->depth, InputOutput, vi->visual, 
        CWBorderPixel | CWColormap | CWEventMask, &swa);
    if (!win) {
      std::cerr << "Failed to create window" << std::endl;
    }

    XFree(vi);
    if (verbose) {
      XMapWindow(display, win);
    }

    char const *glxExts = glXQueryExtensionsString(display, 
        DefaultScreen(display));

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
      glXGetProcAddressARB((GLubyte const *) "glXCreateContextAttribsARB");

    GLXContext ctx = 0;

    ctxErrorOccurred = false;
    int32_t (*oldHandler)(Display *, XErrorEvent *) = 
      XSetErrorHandler(&ctxErrorHandler);

    if (!isExtensionSupported(glxExts, "GLX_ARB_create_context") ||
        !glXCreateContextAttribsARB) {
      if (verbose) {
        std::clog << "Reverint to old GL context" << std::endl;
      }
      ctx = glXCreateNewContext(display, bestFbConfig, GLX_RGBA_TYPE, 0, True);
    } else {
      int32_t contextAttribs[] =
        {
          GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
          GLX_CONTEXT_MINOR_VERSION_ARB, 0,
          //GLX_CONTEXT_FLAGS_ARB      , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
          None
        };

      ctx = glXCreateContextAttribsARB(display, bestFbConfig, 0, True,
          contextAttribs);

      XSync(display, False);
      if (!ctxErrorOccurred && ctx) {
        if (verbose) {
          std::clog << "Created context" << std::endl;
        }
      } else {
        contextAttribs[1] = 1;
        contextAttribs[3] = 0;

        ctxErrorOccurred = false;

        ctx = glXCreateContextAttribsARB(display, bestFbConfig, 0, True,
            contextAttribs);
      }
    }
    
    XSync(display, False);
    XSetErrorHandler(oldHandler);

    if (ctxErrorOccurred || !ctx) {
      std::cerr << "Failed to create an OpenGL context" << std::endl;
      return -1;
    }

    if (!glXIsDirect(display, ctx)) {
      if (verbose) {
        std::clog << "Indirect GLX rendering context obtained" << std::endl;
      }
    } 

    glXMakeCurrent(display, win, ctx);


    glewInit();

    std::string glVersion(reinterpret_cast<char const *>(
          glGetString(GL_VERSION)));

    XStoreName(display, win, glVersion.c_str());

    GLuint programId;
    GLint mvpId;
    GLint doI420Id;
    {
      /*
      std::string vertexShaderGlsl = R"(#version 430

in layout(location=0) vec3 position0;
in layout(location=1) vec3 color0;
in layout(location=2) vec2 uv0;

uniform mat4 u_mvp;

out vec3 color1;
out vec2 uv1;

void main()
{
  vec4 v = vec4(position0, 1.0);
  gl_Position =  u_mvp * v;
  color1 = color0;
  uv1 = uv0;
})";

      std::string fragmentShaderGlsl = R"(#version 430

in vec3 color1;
in vec2 uv1;

out vec4 color2;

uniform sampler2D mySampler;
uniform bool u_do_i420;

const mat4 rgbToYuv = mat4(
  0.257,  0.439, -0.148, 0.0,
  0.504, -0.368, -0.291, 0.0,
  0.098, -0.071,  0.439, 0.0,
  0.0625, 0.500,  0.500, 1.0
);

void main()
{
  vec3 rgb;
  if (uv1 != vec2(0.0, 0.0)) {
    rgb = texture(mySampler, uv1).rgb;
  } else {
    rgb = color1;
  }
  if (u_do_i420) {
    color2 = rgbToYuv * vec4(rgb, 1.0);
  } else {
    color2 = vec4(rgb, 1.0);
  }
})";
*/
      
      std::string vertexShaderGlsl = R"(#version 120

attribute vec3 position0;
attribute vec3 color0;
attribute vec2 uv0;

uniform mat4 u_mvp;

varying vec3 color1;
varying vec2 uv1;

void main()
{
  vec4 v = vec4(position0, 1.0);
  gl_Position =  u_mvp * v;
  color1 = color0;
  uv1 = uv0;
})";

      std::string fragmentShaderGlsl = R"(#version 120

varying vec3 color1;
varying vec2 uv1;

uniform sampler2D mySampler;
uniform bool u_do_i420;

const mat4 rgbToYuv = mat4(
  0.257,  0.439, -0.148, 0.0,
  0.504, -0.368, -0.291, 0.0,
  0.098, -0.071,  0.439, 0.0,
  0.0625, 0.500,  0.500, 1.0
);

void main()
{
  vec3 rgb;
  if (uv1 != vec2(0.0, 0.0)) {
    rgb = texture2D(mySampler, uv1).rgb;
  } else {
    rgb = color1;
  }
  if (u_do_i420) {
    gl_FragColor = rgbToYuv * vec4(rgb, 1.0);
  } else {
    gl_FragColor = vec4(rgb, 1.0);
  }
})";

      bool shaderError{false};
      programId = buildShaders(vertexShaderGlsl, fragmentShaderGlsl);
      if (programId == 0) {
        std::cerr << "Could not load shaders" << std::endl;
        shaderError = true;
      }
      mvpId = glGetUniformLocation(programId, "u_mvp");
      if (!shaderError && mvpId < 0) {
        std::cerr << "Missing shader uniform 'u_mvp'" << std::endl;
        shaderError = true;
      }
      doI420Id = glGetUniformLocation(programId, "u_do_i420");
      if (!shaderError && doI420Id < 0) {
        std::cerr << "Missing shader uniform 'u_do_i420'" << std::endl;
        shaderError = true;
      }
      if (shaderError) {
        glXMakeCurrent(display, 0, 0);
        glXDestroyContext(display, ctx);

        XDestroyWindow(display, win);
        XFreeColormap(display, cmap);
        XCloseDisplay(display);
        return -1;
      }
    }

    GLuint fbo[2];
    glGenFramebuffers(2, fbo);
    
    GLuint tex[2];
    glGenTextures(2, tex);
    
    GLuint rbo[2];
    glGenRenderbuffers(2, rbo);
    
    for (uint32_t i{0}; i < 2; i++) {
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[i]);

      glBindTexture(GL_TEXTURE_2D, tex[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, 
          GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
      glBindTexture(GL_TEXTURE_2D, 0);

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
          GL_TEXTURE_2D, tex[i], 0);

      glBindRenderbuffer(GL_RENDERBUFFER, rbo[i]); 
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width,
          height);  
      glBindRenderbuffer(GL_RENDERBUFFER, 0);

      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
          GL_RENDERBUFFER, rbo[i]);
    }
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      std::cerr << "Framebuffer not complete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  


    nlohmann::json json;
    {
      std::ifstream i(mapPath + "/map.json");
      i >> json;
    }

    struct MeshInstance {
      std::string name;
      glm::vec3 position;
      float rotation;
      bool visible;

      MeshInstance():
        name(), position(), rotation(), visible() {}

      MeshInstance(std::string a_name, glm::vec3 a_position, float a_rotation,
          bool a_visible): 
        name(a_name), position(a_position), rotation(a_rotation), 
        visible(a_visible) {}
    };
    
    GLuint vbo[2];
    glGenBuffers(2, vbo);

    std::vector<MeshInstance> meshInstances;
    std::map<uint32_t, MeshInstance> meshInstancesFrame;
    std::map<std::string, MeshHandle> meshHandles;
    {
      std::vector<ModelInfo> modelInfo;
      if (json.find("model") != json.end()) {
        for (auto const &j : json["model"]) {
          std::string name = j["name"];
          std::string file = j["file"];
          if (j.find("color") != j.end()) {
            float c0 = j["color"][0];
            float c1 = j["color"][1];
            float c2 = j["color"][2];
            modelInfo.push_back({name, mapPath + "/" + file, 
                glm::vec3(c0, c1, c2)});
          }
          if (j.find("textureFile") != j.end()) {
            std::string textureFile = j["textureFile"];
            modelInfo.push_back({name, mapPath + "/" + file, 
                mapPath + "/" + textureFile});
          }
          if (j.find("instances") != j.end()) {
            for (auto const &i : j["instances"]) {
              float x = i[0];
              float y = i[1];
              float z = i[2];
              float a = i[3];
              meshInstances.push_back({name, glm::vec3(x, y, z), a, true});
            }
          }
          if (j.find("frames") != j.end()) {
            for (auto const &i : j["frames"]) {
              meshInstancesFrame[i] = {name, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f,
                false};
            }
          }
        }
      }
      std::vector<BlockInfo> blockInfo;
      if (json.find("block") != json.end()) {
        for (auto const &j : json["block"]) {
          std::string name = j["name"];
          float d0 = j["dimension"][0];
          float d1 = j["dimension"][1];
          float d2 = j["dimension"][2];
          if (j.find("color") != j.end()) {
            float c0 = j["color"][0];
            float c1 = j["color"][1];
            float c2 = j["color"][2];
            blockInfo.push_back({name, glm::vec3(d0, d1, d2), 
                glm::vec3(c0, c1, c2)});
          }
          if (j.find("textureFile") != j.end()) {
            std::string textureFile = j["textureFile"];
            float t0 = j["textureSize"][0];
            float t1 = j["textureSize"][1];
            blockInfo.push_back({name, glm::vec3(d0, d1, d2), 
                mapPath + "/" + textureFile, glm::vec2(t0, t1)});
          }
          for (auto const &i : j["instances"]) {
            float x = i[0];
            float y = i[1];
            float z = i[2];
            float a = i[3];
            meshInstances.push_back({name, glm::vec3(x, y, z), a, true});
          }
        }
      }
      meshHandles = loadModels(modelInfo, blockInfo, vbo, verbose);
    }

    uint32_t const memSize = width * height * 4;
    cluon::SharedMemory sharedMemoryArgb(nameArgb, memSize);
    cluon::SharedMemory sharedMemoryI420(nameI420, memSize);
    if (verbose) {
      std::clog << "Created shared memory " << nameArgb << " (" << memSize 
        << " bytes) for an ARGB image (width = " << width << ", height = " 
        << height << ")." << std::endl;
      std::clog << "Created shared memory " << nameI420 << " (" << memSize 
        << " bytes) for an I420 image (width = " << width << ", height = " 
        << height << ")." << std::endl;
    }
    
    std::mutex meshInstancesFrameMutex;

    bool hasFrame{false};
    glm::mat4 view(glm::mat4(1.0f));
    std::mutex viewMutex;
    auto onFrame{[&frameId, &mountPos, &mountRot, &view, &viewMutex, &hasFrame,
    &meshInstancesFrame, &meshInstancesFrameMutex](
        cluon::data::Envelope &&envelope)
      {
        std::lock_guard<std::mutex> lock(meshInstancesFrameMutex);
        double hpi = glm::pi<double>() / 2.0;
        auto frame = cluon::extractMessage<opendlv::sim::Frame>(
            std::move(envelope));
        glm::vec3 framePos(frame.x(), frame.y(), frame.z());
        double horizontalAngle = hpi - frame.yaw();

        uint32_t const senderStamp = envelope.senderStamp();
        if (frameId == senderStamp) {
          glm::vec3 position = framePos + mountPos;

          double verticalAngle = 0.0f;//frame.pitch() + mountRot.y;

          glm::vec3 direction(
              std::cos(verticalAngle) * std::sin(horizontalAngle), 
              std::cos(verticalAngle) * std::cos(horizontalAngle),
              std::sin(verticalAngle));

          glm::vec3 right = glm::vec3(std::sin(horizontalAngle + hpi),
              std::cos(horizontalAngle + hpi), 0);
          glm::vec3 up = glm::cross(right, direction);

          glm::vec3 positionFlip(position.x, position.y, position.z);
          view = glm::lookAt(positionFlip, positionFlip + direction, up);

          hasFrame = true;
        }

        if (meshInstancesFrame.find(senderStamp) != meshInstancesFrame.end()) {
          meshInstancesFrame[senderStamp].visible = true;
          meshInstancesFrame[senderStamp].position = framePos;
          meshInstancesFrame[senderStamp].rotation = 
            static_cast<float>(horizontalAngle);
        }
      }};

    glm::mat4 proj = glm::perspective(glm::radians(fovy), aspect, 0.1f, 100.0f);
    auto drawScene{[&hasFrame, &meshInstances, &meshHandles, &mvpId, &proj,
      &view, &viewMutex, &meshInstancesFrame, &meshInstancesFrameMutex]() {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      if (hasFrame) {
        std::lock_guard<std::mutex> lock(meshInstancesFrameMutex);
        std::vector<MeshInstance> mis = meshInstances;
            
        for (auto const &mi : meshInstancesFrame) {
          if (mi.second.visible) {
            mis.push_back(mi.second);
          }
        }

        for (auto const &mi : mis) {
          glm::mat4 model = glm::mat4(1.0f);
          model = glm::translate(model, mi.position);
          model = glm::rotate(model, glm::pi<float>() - mi.rotation, 
              glm::vec3(0.0f, 0.0f, 1.0f));

          glm::mat4 mvp = proj * view * model;
          glUniformMatrix4fv(mvpId, 1, GL_FALSE, &mvp[0][0]);
          
          if (meshHandles[mi.name].textureId != 0) {
            glBindTexture(GL_TEXTURE_2D, meshHandles[mi.name].textureId);
          }
          glBindVertexArray(meshHandles[mi.name].vao);
          glDrawElements(GL_TRIANGLES, meshHandles[mi.name].indexCount,
              GL_UNSIGNED_INT, 
              reinterpret_cast<void *>(meshHandles[mi.name].indexOffset));
          
          glBindVertexArray(0);
        }
      }
    }};

    std::vector<uint8_t> buf(memSize);
    bool flippedY{false};
    auto atFrequency{[&doI420Id, &fbo, &proj, &width, &height, &memSize, &buf,
      &flippedY, &sharedMemoryArgb, &sharedMemoryI420, &drawScene, &display,
      &win, &verbose]() -> bool
      {
        cluon::data::TimeStamp sampleTimeStamp = cluon::time::now();

        if (!flippedY) {
          proj *= glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
          flippedY = true;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glUniform1i(doI420Id, 0);
        drawScene();
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, &buf[0]);
        sharedMemoryArgb.lock();
        sharedMemoryArgb.setTimeStamp(sampleTimeStamp);
        {
          memcpy(sharedMemoryArgb.data(), buf.data(), memSize);
        }
        sharedMemoryArgb.unlock();
        sharedMemoryArgb.notifyAll();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glUniform1i(doI420Id, 1);
        drawScene();
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, &buf[0]);
        sharedMemoryI420.lock();
        sharedMemoryI420.setTimeStamp(sampleTimeStamp);
        {
          memcpy(sharedMemoryI420.data(), buf.data(), memSize);
        }
        sharedMemoryI420.unlock();
        sharedMemoryI420.notifyAll();

        if (verbose) {
          if (flippedY) {
            proj *= glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
            flippedY = false;
          }
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
          glUniform1i(doI420Id, 0);
          drawScene();
  
          glXSwapBuffers(display, win);
        }
        return true;
      }};

    glClearColor(0.2f, 0.2f, 0.2f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glUseProgram(programId);

    cluon::OD4Session od4{cid};
    od4.dataTrigger(opendlv::sim::Frame::ID(), onFrame);
    od4.timeTrigger(freq, atFrequency);
        
    glDeleteFramebuffers(2, fbo);
    glDeleteTextures(2, tex);
    glDeleteRenderbuffers(2, rbo);
    glDeleteBuffers(2, vbo);

    for (auto mi : meshHandles) {
      glDeleteVertexArrays(1, &mi.second.vao);
    }
  
    glXMakeCurrent(display, 0, 0);
    glXDestroyContext(display, ctx);

    XDestroyWindow(display, win);
    XFreeColormap(display, cmap);
    XCloseDisplay(display);
  }

  return retCode;
}
