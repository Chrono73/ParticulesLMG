// Include GLFW
#include <glfw3.h>
extern GLFWwindow* window; // The "extern" keyword here is to access the variable "window" declared in tutorialXXX.cpp. This is a hack to keep the tutorials simple. Please avoid this.

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include "controls.hpp"

glm::mat4 ViewMatrix;
glm::mat4 ProjectionMatrix;

glm::mat4 getViewMatrix(){
	return ViewMatrix;
}
glm::mat4 getProjectionMatrix(){
	return ProjectionMatrix;
}


// Initial position : on +Z
glm::vec3 position = glm::vec3( 0, 0, 5 ); 
// Angle horizontal initial
float AngleHorizontal = 3.14f;
// Angle vertical initial : à 0 pour qu'on puisse initialiser la vue en face du point de génération des particules.
float AngleVertical = 0.0f;
// Initialisation du FoV
float initialFoV = 45.0f;

float vitesse = 3.0f;
float vitesseSouris = 0.005f;



void Update_Matrices(){

	// glfwGetTime is called only once, the first time this function is called
	static double lastTime = glfwGetTime();

	// Compute time difference between current and last frame
	double currentTime = glfwGetTime();
	float deltaTime = float(currentTime - lastTime);

	// Get mouse position
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	// Reset mouse position for next frame
	glfwSetCursorPos(window, 1024/2, 768/2);

	// Compute new orientation
	AngleHorizontal += vitesseSouris * float(1024/2 - xpos );
	AngleVertical   += vitesseSouris * float( 768/2 - ypos );

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	glm::vec3 direction(
		cos(AngleVertical) * sin(AngleHorizontal), 
		sin(AngleVertical),
		cos(AngleVertical) * cos(AngleHorizontal)
	);
	
	// Vecteur Right (cf rapport)
	glm::vec3 right = glm::vec3(
		sin(AngleHorizontal - 3.14f/2.0f), 
		0,
		cos(AngleHorizontal - 3.14f/2.0f)
	);
	
	// Vecteur Up (cf rapport)
	glm::vec3 up = glm::cross( right, direction );

	// Déplacement avant
	if (glfwGetKey( window, GLFW_KEY_UP ) == GLFW_PRESS){
		position += direction * deltaTime * vitesse;
	}
	// Déplacement arrière
	if (glfwGetKey( window, GLFW_KEY_DOWN ) == GLFW_PRESS){
		position -= direction * deltaTime * vitesse;
	}
	// Déplacement latéral droit
	if (glfwGetKey( window, GLFW_KEY_RIGHT ) == GLFW_PRESS){
		position += right * deltaTime * vitesse;
	}
	// Déplacement latéral gauche
	if (glfwGetKey( window, GLFW_KEY_LEFT ) == GLFW_PRESS){
		position -= right * deltaTime * vitesse;
	}

	float FoV = initialFoV;

	// Matrice de projection : rapport 4:3
	ProjectionMatrix = glm::perspective(FoV, 4.0f / 3.0f, 0.1f, 100.0f);
	// Camera matrix
	ViewMatrix       = glm::lookAt(
								position,           // Position de la caméra
								position+direction, // Direction dans laquelle la caméra regarde
								up                  // On regarde dans la direction du vecteur Up
						   );

	// On change la valeur de lastTime pour le prochain appel de la fonction.
	lastTime = currentTime;
}