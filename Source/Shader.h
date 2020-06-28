#pragma once
#include <glad4/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum MatrixType
{
	MODEL,
	VIEW,
	PROJECTION,
	MV,
	VP,
	MVP,
	NORMAL
};

class Shader
{
	struct UniformLocation
	{
		std::string Name;
		GLint Location;

		UniformLocation(std::string name, GLint loc)
		{
			Name = name;
			Location = loc;
		}
	};



	unsigned int Program;
	std::string Name;

	std::vector <std::pair<unsigned int, std::string>> MaterialTextureUnits;
	mutable std::vector <UniformLocation> Locations;
	bool ExpectedMatrices[7];

	unsigned int LoadShader(GLenum type, std::string path, std::string additionalData = std::string());
	void DebugShader(unsigned int);
	GLint FindLocation(std::string) const;
public:
	Shader();
	Shader(std::string, std::string, std::string=std::string());
	std::string GetName();
	std::vector<std::pair<unsigned int, std::string>>* GetMaterialTextureUnits();
	bool ExpectsMatrix(unsigned int);
	void SetName(std::string);
	void SetTextureUnitNames(std::vector<std::pair<unsigned int, std::string>>);
	void AddTextureUnit(unsigned int, std::string);
	void SetExpectedMatrices(std::vector<MatrixType>);
	void AddExpectedMatrix(std::string);
	void LoadShaders(std::string vShaderPath, std::string fShaderPath, std::string gShaderPath = std::string());
	void LoadShadersWithInclData(std::string data, std::string vShaderPath, std::string fShaderPath, std::string gShaderPath = std::string());
	void LoadShadersWithExclData(std::string vShaderData, std::string vShaderPath, std::string fShaderData, std::string fShaderPath, std::string gShaderData = std::string(), std::string gShaderPath = std::string());
	void Uniform1i(std::string, int) const;
	void Uniform1f(std::string, float) const;
	void Uniform2fv(std::string, glm::vec2) const;
	void Uniform3fv(std::string, glm::vec3) const;
	void Uniform4fv(std::string, glm::vec4) const;
	void UniformMatrix3fv(std::string, glm::mat3) const;
	void UniformMatrix4fv(std::string, const glm::mat4&) const;
	void UniformBlockBinding(std::string, unsigned int) const;
	void UniformBlockBinding(unsigned int, unsigned int) const;
	unsigned int GetUniformBlockIndex(std::string) const;
	void Use() const;
};