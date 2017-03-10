#include "Game.h"
#include "Vertex.h"

// For the DirectX Math library
using namespace DirectX;

// Initialize static variables
Game* Game::instance;

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore( 
		hInstance,		   // The application's handle
		"AkshayR's DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{
	if (instance != nullptr)
	{
		delete instance;
		instance = NULL;
	}

	instance = this;

	// Initialize fields
	vertexShader = 0;
	pixelShader = 0;

	// Initialize renderer
	renderer = new Renderer();

	// Initialize InputMgr
	inputMgr = new InputManager();

	// Initialize Camera
	camera = new Camera();

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.");
#endif
}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Delete our simple shader objects, which
	// will clean up their own internal DirectX stuff
	delete vertexShader;
	delete pixelShader;

	//Release texture D3D resources
	if (earthSRV) { earthSRV->Release(); }
	if (crateSRV) { crateSRV->Release(); }
	if (metalSRV) { metalSRV->Release(); }
	if (metalRustSRV) { metalRustSRV->Release(); }
	if (sampler) { sampler->Release(); }

	// Delete renderer
	delete renderer;

	// Delete InputMgr
	delete inputMgr;

	// Delete Camera
	delete camera;

	////Lambda function that deletes a Mesh pointer and sets it to NULL
	//auto deleteAndSetToNull = [](void* x) { delete x; x = NULL; };

	// Delete Mesh objs
	for each (Mesh* mesh in meshObjs)
	{
		delete mesh;
		mesh = NULL;
	}

	meshObjs.clear();

	// Delete Entity Objs
	for each (Entity* entity in entities)
	{
		delete entity;
		entity = NULL;
	}

	entities.clear();

	// Delete Material Objs
	for (Material* mat : materials)
	{
		delete mat;
		mat = NULL;
	}

	materials.clear();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	LoadShaders();
	CreateMatrices();

	// Load textures
	if (S_OK !=
		CreateWICTextureFromFile(
			device,
			context,
			L"./Assets/Textures/earth.jpg",
			0, // We don't need a reference to the raw pixels
			&earthSRV))
		return;

	if (S_OK !=
		CreateWICTextureFromFile(
			device,
			context,
			L"./Assets/Textures/crate.jpg",
			0, // We don't need a reference to the raw pixels
			&crateSRV))
		return;

	if (S_OK !=
		CreateWICTextureFromFile(
			device,
			context,
			L"./Assets/Textures/metalFloor.jpg",
			0, // We don't need a reference to the raw pixels
			&metalSRV))
		return;

	if (S_OK !=
		CreateWICTextureFromFile(
			device,
			context,
			L"./Assets/Textures/metalRust.jpg",
			0, // We don't need a reference to the raw pixels
			&metalRustSRV))
		return;

	// Create a sampler decription
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create sampler from description
	device->CreateSamplerState(&sampDesc, &sampler);

	CreateBasicGeometry();

	

	//Init Light
	DirectionalLight light;
	light.AmbientColor = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.DiffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	light.Direction = XMFLOAT3(1.0f, -1.0f, 0.0f);

	//Init Light 2
	DirectionalLight light2;
	light2.AmbientColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	light2.DiffuseColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	light2.Direction = XMFLOAT3(1.0f, 1.0f, 0.0f);

	dirLights.push_back(light);
	dirLights.push_back(light2);

	//Init Point Light 1
	PointLight pLight;
	pLight.Color = XMFLOAT4(1.0f, 0.57f, 0.17f, 1.0f);
	pLight.Position = XMFLOAT3(2.0f, 0.0f, 0.0f);

	pointLights.push_back(pLight);

	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files using
// my SimpleShader wrapper for DirectX shader manipulation.
// - SimpleShader provides helpful methods for sending
//   data to individual variables on the GPU
// --------------------------------------------------------
void Game::LoadShaders()
{
	vertexShader = new SimpleVertexShader(device, context);
	if (!vertexShader->LoadShaderFile(L"Debug/VertexShader.cso"))
		vertexShader->LoadShaderFile(L"VertexShader.cso");		

	pixelShader = new SimplePixelShader(device, context);
	if(!pixelShader->LoadShaderFile(L"Debug/PixelShader.cso"))
		pixelShader->LoadShaderFile(L"PixelShader.cso");

	// You'll notice that the code above attempts to load each
	// compiled shader file (.cso) from two different relative paths.

	// This is because the "working directory" (where relative paths begin)
	// will be different during the following two scenarios:
	//  - Debugging in VS: The "Project Directory" (where your .cpp files are) 
	//  - Run .exe directly: The "Output Directory" (where the .exe & .cso files are)

	// Checking both paths is the easiest way to ensure both 
	// scenarios work correctly, although others exist
}



// --------------------------------------------------------
// Initializes the matrices necessary to represent our geometry's 
// transformations and our 3D camera
// --------------------------------------------------------
void Game::CreateMatrices()
{
	//// Set up world matrix
	//// - In an actual game, each object will need one of these and they should
	////   update when/if the object moves (every frame)
	//// - You'll notice a "transpose" happening below, which is redundant for
	////   an identity matrix.  This is just to show that HLSL expects a different
	////   matrix (column major vs row major) than the DirectX Math library
	//XMMATRIX W = XMMatrixIdentity();
	//XMStoreFloat4x4(&worldMatrix, XMMatrixTranspose(W)); // Transpose for HLSL!

	//// Create the View matrix
	//// - In an actual game, recreate this matrix every time the camera 
	////    moves (potentially every frame)
	//// - We're using the LOOK TO function, which takes the position of the
	////    camera and the direction vector along which to look (as well as "up")
	//// - Another option is the LOOK AT function, to look towards a specific
	////    point in 3D space
	//XMVECTOR pos = XMVectorSet(0, 0, -5, 0);
	//XMVECTOR dir = XMVectorSet(0, 0, 1, 0);
	//XMVECTOR up  = XMVectorSet(0, 1, 0, 0);
	//XMMATRIX V   = XMMatrixLookToLH(
	//	pos,     // The position of the "camera"
	//	dir,     // Direction the camera is looking
	//	up);     // "Up" direction in 3D space (prevents roll)
	//XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(V)); // Transpose for HLSL!

	//// Create the Projection matrix
	//// - This should match the window's aspect ratio, and also update anytime
	////   the window resizes (which is already happening in OnResize() below)
	//XMMATRIX P = XMMatrixPerspectiveFovLH(
	//	0.25f * 3.1415926535f,		// Field of View Angle
	//	(float)width / height,		// Aspect ratio
	//	0.1f,						// Near clip plane distance
	//	100.0f);					// Far clip plane distance
	//XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(P)); // Transpose for HLSL!
}


// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	std::string pathModifier = "./Assets/Models/";

	materials.push_back(new Material(vertexShader, pixelShader, earthSRV, sampler));	//0
	materials.push_back(new Material(vertexShader, pixelShader, crateSRV, sampler));	//1
	materials.push_back(new Material(vertexShader, pixelShader, metalSRV, sampler));	//2
	materials.push_back(new Material(vertexShader, pixelShader, metalRustSRV, sampler));//3

	meshObjs.push_back(new Mesh(pathModifier + "sphere.obj", device));

	entities.push_back(new Entity(meshObjs[0], materials[0]));

	meshObjs.push_back(new Mesh(pathModifier + "cone.obj", device));

	entities.push_back(new Entity(meshObjs[1], materials[2]));

	meshObjs.push_back(new Mesh(pathModifier + "cylinder.obj", device));

	entities.push_back(new Entity(meshObjs[2], materials[2]));

	meshObjs.push_back(new Mesh(pathModifier + "helix.obj", device));

	entities.push_back(new Entity(meshObjs[3], materials[3]));

	meshObjs.push_back(new Mesh(pathModifier + "torus.obj", device));

	entities.push_back(new Entity(meshObjs[4], materials[2]));

	meshObjs.push_back(new Mesh(pathModifier + "cube.obj", device));

	entities.push_back(new Entity(meshObjs[5], materials[1]));

}


// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	//// Update our projection matrix since the window size changed
	//XMMATRIX P = XMMatrixPerspectiveFovLH(
	//	0.25f * 3.1415926535f,	// Field of View Angle
	//	(float)width / height,	// Aspect ratio
	//	0.1f,				  	// Near clip plane distance
	//	100.0f);			  	// Far clip plane distance
	//XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(P)); // Transpose for HLSL!

	// Set projection matrix in camera
	camera->SetProjectionMatrix();
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Quit if the escape key is pressed
	if (GetAsyncKeyState(VK_ESCAPE))
		Quit();

#pragma region EnitityUpdates

	entities[0]->SetTranslation(0.0f, 0.0f, 0.0f);
	entities[1]->SetTranslation(0.0f, 2.0f, 0.0f);
	entities[2]->SetTranslation(0.0f, -2.0f, 0.0f);
	entities[3]->SetTranslation(2.0f, 0.0f, 0.0f);
	entities[4]->SetTranslation(0.0f, 0.0f, 2.0f);
	entities[5]->SetTranslation(-2.0f, 0.0f, 0.0f);

	entities[0]->SetRotation(0.0f, sin(totalTime / 2), 0.0f, cos(totalTime / 2));
	entities[1]->SetRotation(0.0f, sin(totalTime / 2), 0.0f, cos(totalTime / 2));
	entities[2]->SetRotation(0.0f, sin(totalTime / 2), 0.0f, cos(totalTime / 2));
	entities[3]->SetRotation(0.0f, sin(totalTime / 2), 0.0f, cos(totalTime / 2));
	entities[4]->SetRotation(0.0f, sin(totalTime / 2), 0.0f, cos(totalTime / 2));
	entities[5]->SetRotation(0.0f, sin(totalTime / 2), 0.0f, cos(totalTime / 2));

#pragma endregion
	
	camera->Update(deltaTime, totalTime);
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = {0.4f, 0.6f, 0.75f, 0.0f};

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV, color);
	context->ClearDepthStencilView(
		depthStencilView, 
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

#pragma region Old Code

	//// Send data to shader variables
	////  - Do this ONCE PER OBJECT you're drawing
	////  - This is actually a complex process of copying data to a local buffer
	////    and then copying that entire buffer to the GPU.  
	////  - The "SimpleShader" class handles all of that for you.
	//renderer->SetWorldMatrices(entities, vertexShader);		// SETS ONLY WORLD MATRIX for all entities
	//renderer->SetWorldMatrix(entities[0], vertexShader);

	//for (std::vector<Entity*>::iterator it = entities.begin(); it != entities.end(); ++it)
	//{
	//	vertexShader->SetMatrix4x4("world", worldMatrix);
	//	vertexShader->SetMatrix4x4("view", viewMatrix);
	//	vertexShader->SetMatrix4x4("projection", projectionMatrix);
	//}

	//// Once you've set all of the data you care to change for
	//// the next draw call, you need to actually send it to the GPU
	////  - If you skip this, the "SetMatrix" calls above won't make it to the GPU!
	//vertexShader->CopyAllBufferData();

	//// Set the vertex and pixel shaders to use for the next Draw() command
	////  - These don't technically need to be set every frame...YET
	////  - Once you start applying different shaders to different objects,
	////    you'll need to swap the current shaders before each draw
	//vertexShader->SetShader();
	//pixelShader->SetShader();

	//for (std::vector<Entity*>::iterator it = entities.begin(); it != entities.end(); ++it)
	//{
	//	// Set buffers in the input assembler
	//	//  - Do this ONCE PER OBJECT you're drawing, since each object might
	//	//    have different geometry.
	//	UINT stride = sizeof(Vertex);
	//	UINT offset = 0;

	//	ID3D11Buffer* vPointer = nullptr;
	//	if (*it == nullptr)
	//		continue;

	//	vPointer = (*it)->GetMesh()->GetVertexBuffer();
	//	context->IASetVertexBuffers(0, 1, &vPointer, &stride, &offset);
	//	context->IASetIndexBuffer((*it)->GetMesh()->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);

	//	// Finally do the actual drawing
	//	//  - Do this ONCE PER OBJECT you intend to draw
	//	//  - This will use all of the currently set DirectX "stuff" (shaders, buffers, etc)
	//	//  - DrawIndexed() uses the currently set INDEX BUFFER to look up corresponding
	//	//     vertices in the currently set VERTEX BUFFER
	//	context->DrawIndexed(
	//		(*it)->GetMesh()->GetIndexCount(),     // The number of indices to use (we could draw a subset if we wanted)
	//		0,     // Offset to the first index we want to use
	//		0);    // Offset to add to each index when looking up vertices
	//}
#pragma endregion
	//Set View and Projection matrices
	vertexShader->SetMatrix4x4("view", camera->GetViewMatrix());
	vertexShader->SetMatrix4x4("projection", camera->GetProjectionMatrix());

	renderer->Draw(entities, context, camera->GetViewMatrix(), camera->GetProjectionMatrix(), &dirLights[0], &pointLights[0]);

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);
}


#pragma region Mouse Input

// --------------------------------------------------------
// Helper method for mouse clicking.  We get this information
// from the OS-level messages anyway, so these helpers have
// been created to provide basic mouse input if you want it.
// --------------------------------------------------------
void Game::OnMouseDown(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...

	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;

	// Caputure the mouse so we keep getting mouse move
	// events even if the mouse leaves the window.  we'll be
	// releasing the capture once a mouse button is released
	SetCapture(hWnd);
}

// --------------------------------------------------------
// Helper method for mouse release
// --------------------------------------------------------
void Game::OnMouseUp(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...

	// We don't care about the tracking the cursor outside
	// the window anymore (we're not dragging if the mouse is up)
	ReleaseCapture();
}

// --------------------------------------------------------
// Helper method for mouse movement.  We only get this message
// if the mouse is currently over the window, or if we're 
// currently capturing the mouse.
// --------------------------------------------------------
void Game::OnMouseMove(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...
	if (buttonState & 0x0001)	// Left button is down
	{
		camera->MoveSideways((x - prevMousePos.x) * 0.005f);
		camera->MoveVertical((y - prevMousePos.y) * 0.005f);
	}
	if (buttonState & 0x0002)	// Right button is down
	{
		camera->SetRotationY(x - prevMousePos.x);
		camera->SetRotationX(y - prevMousePos.y);
	}

	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;
}

// --------------------------------------------------------
// Helper method for mouse wheel scrolling.  
// WheelDelta may be positive or negative, depending 
// on the direction of the scroll
// --------------------------------------------------------
void Game::OnMouseWheel(float wheelDelta, int x, int y)
{
	// Add any custom code here...

	camera->MoveAlongDirection(wheelDelta * 0.1f);
}

std::vector<Entity*> Game::GetEntities()
{
	return entities;
}
Game * Game::Instance()
{
	return instance;
}
unsigned int Game::GetScreenWidth()
{
	return width;
}
unsigned int Game::GetScreenHeight()
{
	return height;
}
XMFLOAT3 & Game::GetCameraPostion()
{
	// TODO: insert return statement here
	return camera->GetPosition();
}
#pragma endregion