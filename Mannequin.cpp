//  ========================================================================
//  COSC422: Advanced Computer Graphics;  University of Canterbury (2019)
//
//  FILE NAME: ModelViewer.cpp
//  AUTHOR: Alex Tompkins
//  Part I of Assignment #2
//  
//  Press key '1' to toggle 90 degs model rotation about x-axis on/off.
//  ========================================================================

#include <iostream>
#include <map>
#include <GL/freeglut.h>
#include <IL/il.h>

using namespace std;

#include <assimp/cimport.h>
#include <assimp/types.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "assimp_extras.h"

// CONSTANTS
#define TO_RAD (3.14159265f/180.0f)
#define FLOOR_SIZE 10
#define TILE_SIZE 1

struct meshInit {
    int mNumVertices;
    aiVector3D* mVertices;
    aiVector3D* mNormals;
};
meshInit* initData;

struct EyePos {
    float angle = 0.0;
    float rad = 3.0;
    float height = 0.5;
} eyePos;

//----------Globals----------------------------
const aiScene *sceneModel = NULL;
const aiScene *sceneAnim = NULL;
aiVector3D scene_min, scene_max, scene_center;
bool modelRotn = false;
std::map<int, int> texIdMap;

int tDuration;  // Animation duration in ticks
int currTick = 0;
float timeStep = 50.0;  // Animation time step in ms

//------------Modify the following as needed----------------------
float materialCol[4] = {0.5, 0.9, 0.9, 1};   //Default material colour (not used if model's colour is available)
bool replaceCol = false;                       //Change to 'true' to set the model's colour to the above colour
float lightPosn[4] = {0, 50, 50, 1};         //Default light's position
bool twoSidedLight = false;                       //Change to 'true' to enable two-sided lighting

//-------Loads model data from file and creates a scene object----------
bool loadModel(const char *fileName) {
    sceneModel = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality);
    sceneAnim = aiImportFile("./models/Mannequin/run.fbx", aiProcessPreset_TargetRealtime_MaxQuality);
    if (sceneModel == NULL || sceneAnim == NULL){
        cout << "The model file '" << fileName << "' could not be loaded." << endl;
        exit(1);
    }
    printSceneInfo(sceneModel);
    printSceneInfo(sceneAnim);
//    printMeshInfo(sceneModel);
//    printTreeInfo(sceneModel->mRootNode);
//    printBoneInfo(sceneModel);
//    printAnimInfo(sceneAnim);  //WARNING:  This may generate a lengthy output if the model has animation data

    tDuration = sceneAnim->mAnimations[0]->mDuration;
    initData = new meshInit[sceneModel->mNumMeshes];
    for (int meshId = 0; meshId < sceneModel->mNumMeshes; meshId++) {
        aiMesh* mesh = sceneModel->mMeshes[meshId];
        meshInit* initDataMesh = (initData + meshId);
        initDataMesh->mNumVertices = mesh->mNumVertices;
        initDataMesh->mVertices = new aiVector3D[mesh->mNumVertices];
        initDataMesh->mNormals = new aiVector3D[mesh->mNumVertices];

        for (int vertId = 0; vertId < mesh->mNumVertices; vertId++) {
            initDataMesh->mVertices[vertId] = mesh->mVertices[vertId];
            initDataMesh->mNormals[vertId] = mesh->mNormals[vertId];
        }
    }

    return true;
}

string makePathRelative(char* path) {
    string filename = strrchr(path, '/');
    string relative = "./models/Mannequin" + filename;
    return relative;
}

//-------------Loads texture files using DevIL library-------------------------------
void loadGLTextures(const aiScene *scene) {
    /* initialization of DevIL */
    ilInit();
    if (scene->HasTextures()) {
        std::cout << "Support for meshes with embedded textures is not implemented" << endl;
        return;
    }

    /* scan scene's materials for textures */
    /* Simplified version: Retrieves only the first texture with index 0 if present*/
    for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {
        aiString path;  // filename

        if (scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
            glEnable(GL_TEXTURE_2D);
            ILuint imageId;
            GLuint texId;
            ilGenImages(1, &imageId);
            glGenTextures(1, &texId);
            texIdMap[m] = texId;   //store tex ID against material id in a hash map
            ilBindImage(imageId); /* Binding of DevIL image name */
            ilEnable(IL_ORIGIN_SET);
            ilOriginFunc(IL_ORIGIN_LOWER_LEFT);

            string relativePath = makePathRelative(path.data);
            char imagePath[relativePath.size() + 1];
            strcpy(imagePath, relativePath.c_str());

            if (ilLoadImage((ILstring) imagePath))   //if success
            {
                /* Convert image to RGBA */
                ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

                /* Create and load textures to OpenGL */
                glBindTexture(GL_TEXTURE_2D, texId);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH),
                             ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             ilGetData());
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                cout << "Texture:" << imagePath << " successfully loaded." << endl;
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
                glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            } else {
                cout << "Couldn't load Image: " << imagePath << endl;
            }
            glDisable(GL_TEXTURE_2D);
        }
    }  //loop for material

}

// ------A recursive function to traverse scene graph and render each mesh----------
void render(const aiScene *sc, const aiNode *nd) {
    aiMatrix4x4 m = nd->mTransformation;
    aiMesh *mesh;
    aiFace *face;
    aiMaterial *mtl;
    GLuint texId;
    aiColor4D diffuse;
    int meshIndex, materialIndex;

    aiTransposeMatrix4(&m);   //Convert to column-major order
    glPushMatrix();
    glMultMatrixf((float *) &m);   //Multiply by the transformation matrix for this node

    // Draw all meshes assigned to this node
    for (int n = 0; n < nd->mNumMeshes; n++) {
        meshIndex = nd->mMeshes[n];          //Get the mesh indices from the current node
        mesh = sceneModel->mMeshes[meshIndex];    //Using mesh index, get the mesh object

        materialIndex = mesh->mMaterialIndex;  //Get material index attached to the mesh

        if (mesh->HasTextureCoords(0)) {
            glEnable(GL_TEXTURE_2D);
            texId = texIdMap[materialIndex];
            glBindTexture(GL_TEXTURE_2D, texId);
        }

        mtl = sc->mMaterials[materialIndex];
        if (replaceCol)
            glColor4fv(materialCol);   //User-defined colour
        else if (AI_SUCCESS ==
                 aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))  //Get material colour from model
            glColor4f(diffuse.r, diffuse.g, diffuse.b, 1.0);
        else
            glColor4fv(materialCol);   //Default material colour


        //Get the polygons from each mesh and draw them
        for (int k = 0; k < mesh->mNumFaces; k++) {
            face = &mesh->mFaces[k];
            GLenum face_mode;

            switch (face->mNumIndices) {
                case 1:
                    face_mode = GL_POINTS;
                    break;
                case 2:
                    face_mode = GL_LINES;
                    break;
                case 3:
                    face_mode = GL_TRIANGLES;
                    break;
                default:
                    face_mode = GL_POLYGON;
                    break;
            }

            glBegin(face_mode);

            for (int i = 0; i < face->mNumIndices; i++) {
                int vertexIndex = face->mIndices[i];

                if (mesh->HasTextureCoords(0))
                    glTexCoord2f(mesh->mTextureCoords[0][vertexIndex].x, mesh->mTextureCoords[0][vertexIndex].y);

                if (mesh->HasVertexColors(0))
                    glColor4fv((GLfloat *) &mesh->mColors[0][vertexIndex]);

                //Assign texture coordinates here

                if (mesh->HasNormals())
                    glNormal3fv(&mesh->mNormals[vertexIndex].x);

                glVertex3fv(&mesh->mVertices[vertexIndex].x);
            }

            glEnd();
        }
    }

    // Draw all children
    for (int i = 0; i < nd->mNumChildren; i++)
        render(sc, nd->mChildren[i]);

    glPopMatrix();
}

//--------------------OpenGL initialization------------------------
void initialise() {
    float ambient[4] = {0.2, 0.2, 0.2, 1.0};  //Ambient light
    float white[4] = {1, 1, 1, 1};            //Light's colour
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
    if (twoSidedLight) glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50);
    glColor4fv(materialCol);
    loadModel("./models/Mannequin/mannequin.fbx");            //<<<-------------Specify input file name here
    loadGLTextures(sceneModel);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(35, 1, 0.01, 1000.0);
}

void updateNodeMatrices(int tick) {
    int index;
    aiAnimation* anim = sceneAnim->mAnimations[0];
    aiMatrix4x4 matPos, matRot, matProd;
    aiMatrix3x3 matRot3;
    aiNode* nd;

    for (int i = 0; i < anim->mNumChannels; i++) {
        matPos = aiMatrix4x4();
        matRot = aiMatrix4x4();
        aiNodeAnim* ndAnim = anim->mChannels[i];

        index = ndAnim->mNumPositionKeys > 1 ? tick : 0;
        if (ndAnim->mNodeName != (aiString) "free3dmodel_skeleton") {
            aiVector3D posn = ndAnim->mPositionKeys[index].mValue;
            matPos.Translation(posn, matPos);
        }

        index = ndAnim->mNumRotationKeys > 1 ? tick : 0;
        aiQuaternion rotn = ndAnim->mRotationKeys[index].mValue;
        matRot3 = rotn.GetMatrix();
        matRot = aiMatrix4x4(matRot3);

        matProd = matPos * matRot;
        nd = sceneAnim->mRootNode->FindNode(ndAnim->mNodeName);
        nd->mTransformation = matProd;
    }
}

aiMatrix4x4 addIgnoreIdentity(aiMatrix4x4 m1, aiMatrix4x4 m2) {
    if (m1 == aiMatrix4x4()) {
        return m2;
    } else if (m2 == aiMatrix4x4()) {
        return m1;
    } else {
        return m1 + m2;
    }
}

void transformVertices() {
    for (int meshId = 0; meshId < sceneModel->mNumMeshes; meshId++) {
        aiMesh* mesh = sceneModel->mMeshes[meshId];

        // Declare and initialise sum arrays for vertex blending
        aiMatrix4x4 vertexSums[mesh->mNumVertices];
        aiMatrix4x4 normalSums[mesh->mNumVertices];
        for (int i = 0; i < mesh->mNumVertices; i++) {
            vertexSums[i] = aiMatrix4x4();
            normalSums[i] = aiMatrix4x4();
        }

        for (int boneId = 0; boneId < mesh->mNumBones; boneId++) {
            aiBone* bone = mesh->mBones[boneId];
            aiNode* node = sceneAnim->mRootNode->FindNode(bone->mName);
            aiMatrix4x4 matrixProduct = bone->mOffsetMatrix;

            while (node != NULL) {
                matrixProduct = node->mTransformation * matrixProduct;
                node = node->mParent;
            }

            aiMatrix4x4 normalMatrix = aiMatrix4x4(matrixProduct);
            normalMatrix.Inverse().Transpose();

            for (int weightId = 0; weightId < bone->mNumWeights; weightId++) {
                int vertexId = bone->mWeights[weightId].mVertexId;
                vertexSums[vertexId] = addIgnoreIdentity(
                        vertexSums[vertexId],
                        matrixProduct * bone->mWeights[weightId].mWeight);
                normalSums[vertexId] = addIgnoreIdentity(
                        normalSums[vertexId],
                        normalMatrix * bone->mWeights[weightId].mWeight);
            }
        }

        for (int vertexId = 0; vertexId < mesh->mNumVertices; vertexId++) {
            aiVector3D vertex = (initData + meshId)->mVertices[vertexId];
            aiVector3D normal = (initData + meshId)->mNormals[vertexId];

            mesh->mVertices[vertexId] = vertexSums[vertexId] * vertex;
            mesh->mNormals[vertexId] = normalSums[vertexId] * normal;
        }
    }
}

//----Timer callback for continuous rotation of the model about y-axis----
void update(int value) {
    updateNodeMatrices(currTick);
    transformVertices();
    if (currTick == 0) {
        get_bounding_box(sceneModel, &scene_min, &scene_max);
    }

    if (currTick >= tDuration) {
        currTick = 0;
    } else {
        currTick++;
    }

    glutTimerFunc(timeStep, update, 0);
    glutPostRedisplay();
}

void special(int key, int x, int y) {
    const float CHANGE_VIEW_ANGLE = 2.0;
    const float RAD_INCR = 0.5;

    switch (key) {
        case GLUT_KEY_LEFT:
            eyePos.angle -= CHANGE_VIEW_ANGLE;
            break;
        case GLUT_KEY_RIGHT:
            eyePos.angle += CHANGE_VIEW_ANGLE;
            break;
        case GLUT_KEY_UP:
            eyePos.rad -= RAD_INCR;
            break;
        case GLUT_KEY_DOWN:
            eyePos.rad += RAD_INCR;
            break;
    }

    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    const float MOVE_DISTANCE = 0.5;

    switch (key) {
        case '1':
            modelRotn = !modelRotn;   //Enable/disable initial model rotation
            break;
        case ' ':
            eyePos.height += MOVE_DISTANCE;
            break;
        case 'x':
            eyePos.height -= MOVE_DISTANCE;
            break;
    }

    glutPostRedisplay();
}

void drawFloor() {
    bool alternateColour = false;

    glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glTranslatef(0, -0.5, 0);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    for (int x = -FLOOR_SIZE; x <= FLOOR_SIZE; x += TILE_SIZE) {
        for (int z = -FLOOR_SIZE; z <= FLOOR_SIZE; z += TILE_SIZE) {
            if (alternateColour) {
                glColor3f(0.6, 0.2, 0.4);
            } else {
                glColor3f(0.8, 0.2, 0.3);
            }
            glVertex3f(x, 0, z);
            glVertex3f(x, 0, z + TILE_SIZE);
            glVertex3f(x + TILE_SIZE, 0, z + TILE_SIZE);
            glVertex3f(x + TILE_SIZE, 0, z);
            alternateColour = !alternateColour;
        }
    }
    glEnd();
    glPopMatrix();
}

//------The main display function---------
//----The model is first drawn using a display list so that all GL commands are
//    stored for subsequent display updates.
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(eyePos.rad * sin(eyePos.angle * TO_RAD), eyePos.height, eyePos.rad * cos(eyePos.angle * TO_RAD),
            0, 0, 0,
            0, 1, 0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosn);

    drawFloor();

    if (modelRotn) glRotatef(90, 1, 0, 0);          //First, rotate the model about x-axis if needed.

    // scale the whole asset to fit into our view frustum
    float tmp = scene_max.x - scene_min.x;
    tmp = aisgl_max(scene_max.y - scene_min.y, tmp);
    tmp = aisgl_max(scene_max.z - scene_min.z, tmp);
    tmp = 1.f / tmp;
    glScalef(tmp, tmp, tmp);

    float xc = (scene_min.x + scene_max.x) * 0.5;
    float yc = (scene_min.y + scene_max.y) * 0.5;
    float zc = (scene_min.z + scene_max.z) * 0.5;
    // center the model
    glTranslatef(-xc, -yc, -zc);

    render(sceneModel, sceneModel->mRootNode);

    glutSwapBuffers();
}


int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(600, 600);
    glutCreateWindow("Model Loader");
    glutInitContextVersion(4, 2);
    glutInitContextProfile(GLUT_CORE_PROFILE);

    initialise();
    glutDisplayFunc(display);
    glutTimerFunc(timeStep, update, 0);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMainLoop();

    aiReleaseImport(sceneModel);
    aiReleaseImport(sceneAnim);
}

