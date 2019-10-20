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
    float height = 1;
} eyePos;

map<string, int> animNodeMap = {
        {"lankle", 17},  // lFoot
        {"rankle", 20},  // rFoot
        {"lknee", 16},  // lShin
        {"rknee", 19},  // rShin
        {"lhip", 15},  // lThigh
        {"rhip", 18},  // rThigh
        {"spine1", 4},  // neck
        {"spine2", 2},  // chest
        {"middle", 1},  // abdomen
        {"neck", 4}  // neck
};

//----------Globals----------------------------
const aiScene *scene = NULL;
const aiScene *sceneWalk = NULL;
aiVector3D scene_min, scene_max, scene_center;
bool modelRotn = false;
std::map<int, int> texIdMap;

int animDuration, walkAnimDuration;  // Animation duration in ticks
int currTick = 0;
float timeStep = 50.0;  // Animation time step in ms

//------------Modify the following as needed----------------------
float materialCol[4] = {0.5, 0.9, 0.9, 1};   //Default material colour (not used if model's colour is available)
bool replaceCol = false;                       //Change to 'true' to set the model's colour to the above colour
float lightPosn[4] = {-30, 35, 60, 1};         //Default light's position
bool twoSidedLight = false;                       //Change to 'true' to enable two-sided lighting
bool walkEnabled = false;

//-------Loads model data from file and creates a scene object----------
bool loadModel(const char *fileName) {
    scene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_MaxQuality);
    sceneWalk = aiImportFile("./models/Dwarf/avatar_walk.bvh", aiProcessPreset_TargetRealtime_MaxQuality);
    if (scene == NULL || sceneWalk == NULL){
        cout << "The model file '" << fileName << "' could not be loaded." << endl;
        exit(1);
    } else {
        cout << "Model files successfully loaded." << endl;
    }
//    printSceneInfo(scene);
//    printSceneInfo(sceneWalk);
//    printMeshInfo(scene);
//    printMeshInfo(sceneWalk);
//    printTreeInfo(scene->mRootNode);
//    printBoneInfo(scene);
//    printBoneInfo(sceneWalk);
//    printAnimInfo(scene);  //WARNING:  This may generate a lengthy output if the model has animation data

    animDuration = scene->mAnimations[0]->mDuration;
    walkAnimDuration = sceneWalk->mAnimations[0]->mDuration;
    initData = new meshInit[scene->mNumMeshes];
    for (int meshId = 0; meshId < scene->mNumMeshes; meshId++) {
        aiMesh* mesh = scene->mMeshes[meshId];
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
//    string filename = strrchr(path, '/');
    string relative = "./models/Dwarf/" + (string) path;
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
void render(const aiScene *sc, const aiNode *nd, bool isShadow) {
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
        mesh = scene->mMeshes[meshIndex];    //Using mesh index, get the mesh object

        materialIndex = mesh->mMaterialIndex;  //Get material index attached to the mesh

        if (mesh->HasTextureCoords(0)) {
            glEnable(GL_TEXTURE_2D);
            texId = texIdMap[materialIndex];
            glBindTexture(GL_TEXTURE_2D, texId);
        }

        mtl = sc->mMaterials[materialIndex];
        if (isShadow)
            glColor4f(0, 0, 0, 1.0);
        else if (replaceCol)
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

                if (mesh->HasTextureCoords(0) && !isShadow)
                    glTexCoord2f(mesh->mTextureCoords[0][vertexIndex].x, mesh->mTextureCoords[0][vertexIndex].y);

                if (mesh->HasVertexColors(0) && !isShadow)
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
        render(sc, nd->mChildren[i], isShadow);

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
    loadModel("./models/Dwarf/dwarf.x");            //<<<-------------Specify input file name here
    loadGLTextures(scene);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(35, 1, 0.01, 1000.0);
}

aiVector3D findValueForTick(int tick, aiVectorKey *keys, int numKeys) {
    aiVectorKey key, prevKey;
    for (int i = 1; i < numKeys; i++) {
        key = keys[i];
        prevKey = keys[i - 1];
        if (prevKey.mTime < tick && tick <= key.mTime) {
            return key.mValue;
        }
    }
    return keys[0].mValue;
}

aiQuaternion interpolateRotnKeys(aiQuatKey k1, aiQuatKey k2, int tick) {
    double factor = (tick - k1.mTime) / (k2.mTime - k1.mTime);
    aiQuaternion rotn = aiQuaternion();
    rotn.Interpolate(rotn, k1.mValue, k2.mValue, factor);
    return rotn;
}

aiQuaternion findValueForTick(int tick, aiQuatKey *keys, int numKeys) {
    aiQuatKey key, prevKey;
    for (int i = 0; i < numKeys; i++) {
        key = keys[i];
        if (key.mTime == tick) {
            return key.mValue;
        }
        prevKey = keys[i - 1];
        if (prevKey.mTime < tick && tick <= key.mTime) {
            return interpolateRotnKeys(prevKey, key, tick);
        }
    }
    return keys[0].mValue;
}

aiQuaternion getRotation(aiNodeAnim* targetAnimNode, int tick) {
    aiNodeAnim* rotnAnim;
    aiString name = targetAnimNode->mNodeName;

    if (walkEnabled && animNodeMap.find(name.data) != animNodeMap.end()) {
        int channelNum = animNodeMap[name.data];
//        cout << "Found a matching channel for '" << name.data << "': " << channelNum << endl;
        rotnAnim = sceneWalk->mAnimations[0]->mChannels[channelNum];
        tick = tick % walkAnimDuration + 1;
    } else {
//        cout << "Couldn't find a matching channel for '" << name.data << "', using original animation data." << endl;
        rotnAnim = targetAnimNode;
        tick = tick % animDuration + 1;
    }

    return findValueForTick(tick, rotnAnim->mRotationKeys, rotnAnim->mNumRotationKeys);
}

void updateNodeMatrices(int tick) {
    aiAnimation* anim = scene->mAnimations[0];
    aiMatrix4x4 matPos, matRot, matProd;
    aiMatrix3x3 matRot3;
    aiNode* nd;

    for (int i = 0; i < anim->mNumChannels; i++) {
        aiNodeAnim* ndAnim = anim->mChannels[i];

        aiVector3D posn;
        if (walkEnabled) {
            posn = ndAnim->mPositionKeys[0].mValue;
        } else {
            posn = findValueForTick(tick % animDuration + 1, ndAnim->mPositionKeys, ndAnim->mNumPositionKeys);
        }
        matPos = aiMatrix4x4();
        matPos.Translation(posn, matPos);

        aiQuaternion rotn = getRotation(ndAnim, tick);
        matRot3 = rotn.GetMatrix();
        matRot = aiMatrix4x4(matRot3);

        matProd = matPos * matRot;
        nd = scene->mRootNode->FindNode(ndAnim->mNodeName);
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
    for (int meshId = 0; meshId < scene->mNumMeshes; meshId++) {
        aiMesh* mesh = scene->mMeshes[meshId];

        // Declare and initialise sum arrays for vertex blending
        aiMatrix4x4 vertexSums[mesh->mNumVertices];
        aiMatrix4x4 normalSums[mesh->mNumVertices];
        for (int i = 0; i < mesh->mNumVertices; i++) {
            vertexSums[i] = aiMatrix4x4();
            normalSums[i] = aiMatrix4x4();
        }

        for (int boneId = 0; boneId < mesh->mNumBones; boneId++) {
            aiBone* bone = mesh->mBones[boneId];
            aiNode* node = scene->mRootNode->FindNode(bone->mName);
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
        get_bounding_box(scene, &scene_min, &scene_max);
    }

    currTick++;
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
            walkEnabled = false;
            break;
        case '2':
            walkEnabled = true;
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
}

//------The main display function---------
//----The model is first drawn using a display list so that all GL commands are
//    stored for subsequent display updates.
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(eyePos.rad * sin(eyePos.angle * TO_RAD), eyePos.height, eyePos.rad * cos(eyePos.angle * TO_RAD),
            0, 0.5, 0,
            0, 1, 0);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosn);

    glPushMatrix();
    drawFloor();
    glPopMatrix();

    // Draw planar shadow
    glDisable(GL_LIGHTING);
    glPushMatrix();

    glTranslatef(0, 0.01, 0);
    float shadowMat[16] = {
            lightPosn[1], 0, 0, 0,
            -lightPosn[0], 0, -lightPosn[2], -1,
            0, 0, lightPosn[1], 0,
            0, 0, 0, lightPosn[1]
    };
    glMultMatrixf(shadowMat);

    if (modelRotn) glRotatef(90, 1, 0, 0);          //First, rotate the model about x-axis if needed.
    // scale the whole asset to fit into our view frustum
    float tmp = scene_max.x - scene_min.x;
    tmp = aisgl_max(scene_max.y - scene_min.y, tmp);
    tmp = aisgl_max(scene_max.z - scene_min.z, tmp);
    tmp = 1.f / tmp;
    glScalef(tmp, tmp, tmp);

    render(scene, scene->mRootNode, true);
    glPopMatrix();

    // Draw object
    glEnable(GL_LIGHTING);
    glPushMatrix();
    if (modelRotn) glRotatef(90, 1, 0, 0);          //First, rotate the model about x-axis if needed.

    // scale the whole asset to fit into our view frustum
    tmp = scene_max.x - scene_min.x;
    tmp = aisgl_max(scene_max.y - scene_min.y, tmp);
    tmp = aisgl_max(scene_max.z - scene_min.z, tmp);
    tmp = 1.f / tmp;
    glScalef(tmp, tmp, tmp);

    render(scene, scene->mRootNode, false);
    glPopMatrix();

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

    aiReleaseImport(scene);
}

