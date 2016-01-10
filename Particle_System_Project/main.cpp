#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <algorithm>

#include <GL/glew.h>

#include <glfw3.h>
GLFWwindow* window;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
using namespace glm;


#include <common/shader.hpp>
#include <common/texture.hpp>
#include <common/controls.hpp>

//Definition d'une structure pour les particules :
struct Particule{
	glm::vec3 position, vitesse;
	unsigned char r,g,b,a; // Couleur de la particule
	float taille, angle, poids;//On se servira de poids pour implémenter un comportement lié à la gravité.
	float vie; //Vie restante de la particule. Si <0, la particule est morte : elle est donc inutilisée.
	float cameradistance; // Distance (au carré) à la caméra. Si la particule est morte, distanceCamera = -1.0f

	bool operator<(const Particule& that) const {
		// On dessine en premier les particules les plus éloignées.
		return this->cameradistance > that.cameradistance;
	}
};

const int Particule_Max = 100000;
Particule Contenant_Particule[Particule_Max];
int Derniere_Particule = 0;

//====================================================================================================================================
//Cet espace va nous servir à créer différentes fonctions pour manipuler le Contenant_Particule crée plus haut.


int Trouve_Derniere_Particule(){
	//Deux étapes dans cette fonction pour trouver la derniere particule :
	//La premiere : on parcourt le contenant de l'indice Derniere_Particule jusqu'à Particule_Max
	for(int i=Derniere_Particule; i<Particule_Max; i++){
		if (Contenant_Particule[i].vie < 0){
			Derniere_Particule = i;
			return i;
		}
	}
	//La deuxième : on parcourt le contenant de l'indice 0 jusqu'à Derniere_Particule
	for(int i=0; i<Derniere_Particule; i++){
		if (Contenant_Particule[i].vie < 0){
			Derniere_Particule = i;
			return i;
		}
	}

	return 0; //Dans le cas ou toute les particules sont prises, on renvoie l'indice de la première.
}

void Tri_Particule(){
	//Trie les particules dans le tableau.
	std::sort(&Contenant_Particule[0], &Contenant_Particule[Particule_Max]);
}

int main( void )
{
	// Initialise GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_RESIZABLE,GL_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Création d'une fenêtre et mise en place du contexte courant
	window = glfwCreateWindow( 1024, 768, "Particle System Project", NULL, NULL);
	if( window == NULL ){
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwPollEvents();
    glfwSetCursorPos(window, 1024/2, 768/2);

	// COuleur de l'arrière-plan (TODO : changer la couleur du background via un fichier)
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);


	// Création et compilation des shaders (code dans shader.cpp)
	GLuint programID = LoadShaders( "Particle.vertexshader", "Particle.fragmentshader" );

	// Uniforms pour le vertex shader
	GLuint CameraRight_worldspace_ID  = glGetUniformLocation(programID, "CameraRight_worldspace");
	GLuint CameraUp_worldspace_ID  = glGetUniformLocation(programID, "CameraUp_worldspace");
	GLuint ViewProjMatrixID = glGetUniformLocation(programID, "VP");

	// Uniform pour le fragment shader
	GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");

	//Initialisation des tableaux de données de position et de couleur
	static GLfloat* donnees_particule_position_taille = new GLfloat[Particule_Max * 4];
	static GLubyte* donnees_particule_couleur         = new GLubyte[Particule_Max * 4];
	//Initialisation des particules : elles commencent toutes mortes.
	for(int i=0; i<Particule_Max; i++){
		Contenant_Particule[i].vie = -1.0f;
		Contenant_Particule[i].cameradistance = -1.0f;
	}


	//Récupération du DDS qui servira de texture pour la particule (code de loadDDS() dans texture.cpp)
	GLuint Texture = loadDDS("particle.DDS");

	//Définition des 3 buffers nécéssaires à la gestion du système de particules : le premier contiendra les informations de vertices pour chaque particule.
	//Les particules étant générées via instanciation, chaque particule aura les mêmes données de vertices.
	static const GLfloat donnees_buffer_vertices[] = {
		 -0.5f, -0.5f, 0.0f,
		  0.5f, -0.5f, 0.0f,
		 -0.5f,  0.5f, 0.0f,
		  0.5f,  0.5f, 0.0f,
	};
	GLuint buffer_vertices;
	glGenBuffers(1, &buffer_vertices);
	glBindBuffer(GL_ARRAY_BUFFER, buffer_vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(donnees_buffer_vertices), donnees_buffer_vertices, GL_STATIC_DRAW);

	//Le deuxieme buffer contient les données relatives à la position et à la taille de chaque particule.
	//Il est initialement vide, et est mis à jour à chaque frame.
	GLuint buffer_particule_position_taille;
	glGenBuffers(1, &buffer_particule_position_taille);
	glBindBuffer(GL_ARRAY_BUFFER, buffer_particule_position_taille);
	glBufferData(GL_ARRAY_BUFFER, Particule_Max * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

	//Le dernier buffer contient les données relatives à la couleur de chaque particule.
	//Tout comme le buffer de position et taille, il est mis à jour à chaque frame et est initialisé vide.
	GLuint buffer_particule_couleur;
	glGenBuffers(1, &buffer_particule_couleur);
	glBindBuffer(GL_ARRAY_BUFFER, buffer_particule_couleur);
	glBufferData(GL_ARRAY_BUFFER, Particule_Max * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW);


	
	double lastTime = glfwGetTime();
	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		double currentTime = glfwGetTime();
		double delta = currentTime - lastTime;
		lastTime = currentTime;


		Update_Matrices();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();

		//On a besoin de la distance à la caméra pour trier par la suite les particules
		glm::vec3 CameraPosition(glm::inverse(ViewMatrix)[3]);
		//On calcule la matrice VP, utile par la suite.
		glm::mat4 ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;


		//On génère 1 particule par milliseconde, en limitant la génération 
		//à 16 millisecondes (ce qui correspond à peu près à 60 FPS).
		int newparticles = (int)(delta*10000.0);
		if (newparticles > (int)(0.016f*10000.0))
			newparticles = (int)(0.016f*10000.0);

		//A chaque nouvelle particule, on affecte un temps de vie et une position.
		for(int i=0; i<newparticles; i++){
			int particleIndex = Trouve_Derniere_Particule();
			Contenant_Particule[particleIndex].vie = 5.0f; // TODO : changer la durée de vie des particules
			Contenant_Particule[particleIndex].position = glm::vec3(0,0,-20.0f);

			float spread = 1.5f; //TODO : changer la valeur du spread via un fichier
			glm::vec3 maindir = glm::vec3(0.0f, 10.0f, 0.0f);
			// TODO : coder générateur de direction aléatoire plus joli
				glm::vec3 randomdir = glm::vec3(
				(rand()%2000 - 1000.0f)/1000.0f,
				(rand()%2000 - 1000.0f)/1000.0f,
				(rand()%2000 - 1000.0f)/1000.0f
			);
			if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
			{
				float BlastForce = 4; //A modifier via JSon
				Contenant_Particule[particleIndex].vitesse = randomdir * BlastForce;
			}
			else
			{
				Contenant_Particule[particleIndex].vitesse = maindir + randomdir*spread;
			}

			// TODO : choisir couleur des particules
			Contenant_Particule[particleIndex].r = 255;
			Contenant_Particule[particleIndex].g = 255;
			Contenant_Particule[particleIndex].b = 255;
			Contenant_Particule[particleIndex].a = (rand() % 256) / 3;

			Contenant_Particule[particleIndex].taille = (rand()%1000)/2000.0f + 0.1f;
			
		}



		// On peut maintenant commencer à simuler toutes les particules de Contenant_Particule
		int ParticlesCount = 0;
		for(int i=0; i<Particule_Max; i++){

			Particule& p = Contenant_Particule[i];

			if(p.vie > 0.0f){

				// On décrémente la vie de nos particules encore vivantes
				p.vie -= delta;
				if (p.vie > 0.0f){

					// Simulation de la gravité, sans collision
					//Physique 1 : Fountain (à changer via JSON)
					if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
					{
						p.vitesse += glm::vec3(0.0f, -9.81f, 0.0f) * (float)delta * 0.5f;
						
					}
					//Physique 2 : Blast
					else if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
					{
						//On ne fait rien : la vitesse des particules est constante et déterminée à leur génération.
					}
					else
					{
						p.vitesse += glm::vec3(0.0f, -2.0f, 0.0f) * (float)delta * 0.5f;
					}
					p.position += p.vitesse * (float)delta;
					p.cameradistance = glm::length2(p.position - CameraPosition);
					// Remplissage des tableaux de données avec les nouvelles informations
					donnees_particule_position_taille[4*ParticlesCount+0] = p.position.x;
					donnees_particule_position_taille[4*ParticlesCount+1] = p.position.y;
					donnees_particule_position_taille[4*ParticlesCount+2] = p.position.z;
												   
					donnees_particule_position_taille[4*ParticlesCount+3] = p.taille;
												   
					donnees_particule_couleur[4*ParticlesCount+0] = p.r;
					donnees_particule_couleur[4*ParticlesCount+1] = p.g;
					donnees_particule_couleur[4*ParticlesCount+2] = p.b;
					donnees_particule_couleur[4*ParticlesCount+3] = p.a;

				}else{
					// Dans le cas où la particule est morte, elle sera placé à la fin du buffer grâce à Tri_Particules
					p.cameradistance = -1.0f;
				}

				ParticlesCount++;

			}
		}

		Tri_Particule();


		printf("%d ",ParticlesCount);

		//Mise à jour des différents buffers.
		//La procédure est la même pour les deux : allocation de mémoire via glBufferData, puis affectation des données via glBufferSubData pour gagner en performance. 
		glBindBuffer(GL_ARRAY_BUFFER, buffer_particule_position_taille);
		glBufferData(GL_ARRAY_BUFFER, Particule_Max * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLfloat) * 4, donnees_particule_position_taille);

		glBindBuffer(GL_ARRAY_BUFFER, buffer_particule_couleur);
		glBufferData(GL_ARRAY_BUFFER, Particule_Max * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLubyte) * 4, donnees_particule_couleur);


		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(programID);

		// On bind notre texture sur la TextureUnit0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		// Le "mytexturesampler" utilisera la textureUnit 0
		glUniform1i(TextureID, 0);

		// Mise en place des matrices de Right et Up (voir le rapport)
		glUniform3f(CameraRight_worldspace_ID, ViewMatrix[0][0], ViewMatrix[1][0], ViewMatrix[2][0]);
		glUniform3f(CameraUp_worldspace_ID   , ViewMatrix[0][1], ViewMatrix[1][1], ViewMatrix[2][1]);

		glUniformMatrix4fv(ViewProjMatrixID, 1, GL_FALSE, &ViewProjectionMatrix[0][0]);

		// On peut maintenant utilise notre VAO. On l'utilise pour chacun de nos trois buffers.
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, buffer_vertices);
		glVertexAttribPointer(
			0,                  // attribut (pour le shader)
			3,                  // taille
			GL_FLOAT,           // type
			GL_FALSE,           
			0,                  
			(void*)0            
		);
		
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, buffer_particule_position_taille);
		glVertexAttribPointer(
			1,                                // attribut (pour le shader)
			4,                                // 4 variables : x,y,z et taille
			GL_FLOAT,                         // type
			GL_FALSE,                         
			0,                                
			(void*)0                          
		);

		// 3rd attribute buffer : particles' colors
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, buffer_particule_couleur);
		glVertexAttribPointer(
			2,                                // attribut (pour le shader)
			4,                                // 4 variables : r, g, b et a.
			GL_UNSIGNED_BYTE,                 // type
			GL_TRUE,                          
			0,                                
			(void*)0                          
		);

		//Fonction glVertexAttribDivisor : 
		//1er parametre : element du VAO
		//2eme parametre : nombre de fois qu'on souhaite utiliser les données du VAO pour chaque quad
		glVertexAttribDivisor(0, 0); // les vertices sont toujours les mêmes, on ne les change jamais --> 0
		glVertexAttribDivisor(1, 1); // on récupère la position du centre et la taille de chaque quad --> 1
		glVertexAttribDivisor(2, 1); // on récupère la couleur de chaque quad						  -->

		//Dessin des particules
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, ParticlesCount);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // si l'on appuie sur Echap, on ferme le programme.
	while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
		   glfwWindowShouldClose(window) == 0 );


	delete[] donnees_particule_position_taille;
	delete[] donnees_particule_couleur;
	// Cleanup VBO and shader
	glDeleteBuffers(1, &buffer_particule_couleur);
	glDeleteBuffers(1, &buffer_particule_position_taille);
	glDeleteBuffers(1, &buffer_vertices);
	glDeleteProgram(programID);
	glDeleteTextures(1, &TextureID);
	glDeleteVertexArrays(1, &VertexArrayID);
	

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

