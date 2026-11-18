#ifndef SPRITE_MANAGER_H
#define SPRITE_MANAGER_H

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "basic_transform.h"
#include "texture_atlas.h"
#include "vector_util.h"
#include "glad/glad.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#define GL_SILENCE_DEPRECATION  // silences macOS OpenGL deprecation warnings

#define glCheck(expr)                                                                              \
    expr;                                                                                          \
    if (int e = glGetError()) {                                                                    \
        std::cerr << "OpenGL error: " << e << " in file " __FILE__ << ", line number " << __LINE__ \
                  << '\n';                                                                         \
        abort();                                                                                   \
    }

using SpriteID = size_t;

class SpriteManager {
public:
    SpriteManager(const sf::Vector2u& atlasSize, unsigned padding)
        : mVertices(sf::PrimitiveType::Triangles),
        mNumSprites{0},
        mAtlas(atlasSize, padding),
        mShader(std::filesystem::path{"main.vert"}, std::filesystem::path{"main.frag"}),
        mSfmlShader(std::filesystem::path{"main_sfml.vert"}, std::filesystem::path{"main.frag"}) {
        // TODO
        const GLuint program = mShader.getNativeHandle();
        mPositionAttribLocation = glGetAttribLocation(program, "position");
        mTexCoordsAttribLocation = glGetAttribLocation(program, "texCoords");
        mSpriteNumberAttribLocation = glGetAttribLocation(program, "spriteNumber");
        mTextureUniformLocation = glGetUniformLocation(program, "texture");
        mTransformsUniformLocation = glGetUniformLocation(program, "transforms");
        mNumTransformsUniformLocation = glGetUniformLocation(program, "numTransforms");
        if (mPositionAttribLocation < 0 || mTexCoordsAttribLocation < 0 || mSpriteNumberAttribLocation < 0 || mTextureUniformLocation < 0 || mTransformsUniformLocation < 0 || mNumTransformsUniformLocation < 0) {
            throw std::runtime_error("Failed to find SpriteManager shader attributes or uniforms");
        }
        glCheck(glGenBuffers(1, &mVerticesVBO));
        glCheck(glGenBuffers(1, &mSpriteNumberVBO));
        glCheck(glGenTextures(1, &mTransformsTexture));
        mSfmlShader.setUniform("texture", sf::Shader::CurrentTexture);
    }
    ~SpriteManager() {
        glDeleteBuffers(1, &mVerticesVBO);
        glDeleteBuffers(1, &mSpriteNumberVBO);
        glDeleteTextures(1, &mTransformsTexture);
    }

    SpriteManager(const SpriteManager&) = delete;
    SpriteManager& operator=(const SpriteManager&) = delete;
    // TODO
    TextureID registerTexture(const std::string& filepath) { 
        return mAtlas.registerTexture(filepath);
    };

    // TODO
    SpriteID createSprite(TextureID textureID) { 
        const SpriteID id = mNumSprites;
        ++mNumSprites;
        mTransforms.emplace_back();
        mSpriteNumbers.insert(mSpriteNumbers.end(), 6, static_cast<float>(id));
        mVertices.resize(mVertices.getVertexCount() + 6);
        setTexture(id, textureID);
        return id;
     };

    // TODO
    void setTexture(SpriteID sprite, TextureID newTexture) {
        assert(sprite < mNumSprites);
        const sf::FloatRect uv = mAtlas.getBoundsUV(newTexture);
        const sf::Vector2u sizePixels = mAtlas.getSizePixels(newTexture);
        const float width = static_cast<float>(sizePixels.x);
        const float height = static_cast<float>(sizePixels.y);
        const float left = uv.position.x;
        const float right = uv.position.x + uv.size.x;
        const float bottom = uv.position.y;
        const float top = uv.position.y + uv.size.y;
        const sf::Vector2f positions[6] = { {0.f, 0.f}, {width, 0.f}, {width, height}, {0.f, 0.f}, {width, height}, {0.f, height} };
        const sf::Vector2f texCoords[6] = { {left, bottom}, {right, bottom}, {right, top}, {left, bottom}, {right, top}, {left, top}};
        for (size_t i = 0; i < 6; ++i) { 
            sf::Vertex& vertex = mVertices[sprite * 6 + i]; 
            vertex.position = positions[i]; 
            vertex.texCoords = texCoords[i]; 
            vertex.color = sf::Color::White;}
    };

    // TODO
    void draw(sf::RenderTarget& target, sf::RenderStates states) const {
        sf::VertexArray transformed = mVertices;
        for (size_t sprite = 0; sprite < mNumSprites; ++sprite) {
            for (size_t i = 0; i < 6; ++i) {
                sf::Vertex& vertex = transformed[sprite * 6 + i];
                vertex.position = mTransforms[sprite].applyPoint(vertex.position);
            }
        }
        states.texture = &mAtlas.getTexture();
        states.shader = &mSfmlShader;
        states.coordinateType = sf::CoordinateType::Normalized;
        target.draw(transformed, states);
    };

    // TODO
    void drawGL() {
        if (mNumSprites == 0) {
            return;
        }
        glCheck(glUseProgram(mShader.getNativeHandle()));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, mVerticesVBO));
        glCheck(glBufferData(GL_ARRAY_BUFFER, mVertices.getVertexCount() * sizeof(sf::Vertex), &mVertices[0], GL_STREAM_DRAW));
        glCheck(glVertexAttribPointer(static_cast<GLuint>(mPositionAttribLocation), 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex), reinterpret_cast<const void*>(offsetof(sf::Vertex, position))));
        glCheck(glVertexAttribPointer(static_cast<GLuint>(mTexCoordsAttribLocation), 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex), reinterpret_cast<const void*>(offsetof(sf::Vertex, texCoords))));
        glCheck(glEnableVertexAttribArray(static_cast<GLuint>(mPositionAttribLocation)));
        glCheck(glEnableVertexAttribArray(static_cast<GLuint>(mTexCoordsAttribLocation)));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, mSpriteNumberVBO));
        glCheck(glBufferData(GL_ARRAY_BUFFER, mSpriteNumbers.size() * sizeof(float), mSpriteNumbers.data(), GL_STREAM_DRAW));
        glCheck(glVertexAttribPointer(static_cast<GLuint>(mSpriteNumberAttribLocation), 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr));
        glCheck(glEnableVertexAttribArray(static_cast<GLuint>(mSpriteNumberAttribLocation)));
        glCheck(glActiveTexture(GL_TEXTURE1));
        glCheck(glBindTexture(GL_TEXTURE_1D, mTransformsTexture));
        glCheck(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        glCheck(glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F_ARB, static_cast<GLsizei>(mTransforms.size()), 0, GL_RGBA, GL_FLOAT, mTransforms.data()));
        glCheck(glUniform1i(mTransformsUniformLocation, 1));
        glCheck(glUniform1i(mNumTransformsUniformLocation, static_cast<GLint>(mTransforms.size())));
        glCheck(glActiveTexture(GL_TEXTURE0));
        glCheck(glBindTexture(GL_TEXTURE_2D, mAtlas.getTexture().getNativeHandle()));
        glCheck(glUniform1i(mTextureUniformLocation, 0));
        glCheck(glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mVertices.getVertexCount())));
        glCheck(glDisableVertexAttribArray(static_cast<GLuint>(mPositionAttribLocation)));
        glCheck(glDisableVertexAttribArray(static_cast<GLuint>(mTexCoordsAttribLocation)));
        glCheck(glDisableVertexAttribArray(static_cast<GLuint>(mSpriteNumberAttribLocation)));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        glCheck(glUseProgram(0));
    };

    // TODO
    BasicTransform& getTransform(SpriteID id) {
        assert(id < mNumSprites);
        return mTransforms[id];
    }

    // TODO
    const TextureAtlas& getAtlas() const {
        return mAtlas;
    };

private:
    sf::VertexArray mVertices;
    std::vector<BasicTransform> mTransforms;
    size_t mNumSprites;
    TextureAtlas mAtlas;
    mutable sf::Shader mShader;
    mutable sf::Shader mSfmlShader;
    // Use the below only in Batched rendering (part 2):
    std::vector<float> mSpriteNumbers;

    GLint mTextureUniformLocation;
    GLint mTransformsUniformLocation;
    GLint mNumTransformsUniformLocation;

    GLint mPositionAttribLocation;
    GLint mTexCoordsAttribLocation;
    GLint mSpriteNumberAttribLocation;

    GLuint mVerticesVBO;
    GLuint mSpriteNumberVBO;
    GLuint mTransformsTexture;
};

#endif  // SPRITE_MANAGER_H
