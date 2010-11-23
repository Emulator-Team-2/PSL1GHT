
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <rsx/commands.h>
#include <rsx/nv40.h>
#include <rsx/reality.h>

#include <io/pad.h>

#include <sysmodule/sysmodule.h>

#include "dice.bin.h"
#include "texture.h"
#include "rsxutil.h"
#include "nv_shaders.h"
#include "vshader.vcg.h"

typedef struct
{
       float data[4][4];
} realityMatrix;


realityMatrix realityMatrixMul(realityMatrix *left,realityMatrix *right)
{
       int row,col;
       realityMatrix result;

       for(row=0;row<4;++row)
       {
               for(col=0;col<4;++col)
               {
                       int i;
                       result.data[row][col] = 0;
                       for(i=0;i<4;++i)
                       {
                               result.data[row][col]+=left->data[row][i]*right->data[i][col];
                       }
               }
       }

       return result;
}

realityMatrix realityMatrixIdentity()
{
       int row,col;
       realityMatrix result;

       for(row=0;row<4;++row)
       {
               for(col=0;col<4;++col)
               {
                       if(row==col)
                               result.data[row][col]=1;
                       else
                               result.data[row][col]=0;
               }
       }

       return result;
}

realityMatrix realityMatrixTranslation(float x,float y,float z)
{
       realityMatrix result=realityMatrixIdentity();
       result.data[3][0]=x;
       result.data[3][1]=y;
       result.data[3][2]=z;

       return result;
}

realityMatrix realityMatrixScale(float x,float y,float z)
{
       realityMatrix result=realityMatrixIdentity();
       result.data[0][0]=x;
       result.data[1][1]=y;
       result.data[2][2]=z;

       return result;
}

realityMatrix realityMatrixTranspose(realityMatrix *src)
{
       int row,col;

       realityMatrix result;

       for(row=0;row<4;++row)
       {
               for(col=0;col<4;++col)
               {
                       result.data[row][col] = src->data[col][row];
               }
       }
       return result;
}

realityMatrix realityMatrixRotationX(float angle)
{
       realityMatrix result=realityMatrixIdentity();

       result.data[1][1]=cos(angle);
       result.data[2][1]=sin(angle);

       result.data[1][2]=-sin(angle);
       result.data[2][2]=cos(angle);

       return result;
}

realityMatrix realityMatrixRotationY(float angle)
{
       realityMatrix result=realityMatrixIdentity();

       result.data[0][0]=cos(angle);
       result.data[2][0]=-sin(angle);

       result.data[0][2]=sin(angle);
       result.data[2][2]=cos(angle);

       return result;
}

realityMatrix realityMatrixRotationZ(float angle)
{
       realityMatrix result=realityMatrixIdentity();

       result.data[0][0]=cos(angle);
       result.data[1][0]=sin(angle);

       result.data[0][1]=-sin(angle);
       result.data[1][1]=cos(angle);

       return result;
}

int currentBuffer = 0;
//realityTexture *ball; // Texture.

u32 *tx_mem;
u32 tx_offset;
Image dice;

void drawFrame(int buffer, long frame) {

	//Find the constant where the matrix is stored in the shader (shouldn't actually do this every frame :) )
	int matrixParam=realityVertexProgramGetConstant((realityVertexProgram*)vshader_bin,"TransMatrix");
	realityMatrix matrix,tmp;
	float ang=((float)(frame%360))*(M_PI)/180.0;

	//Build a transform matrix to rotate the quad by its center
	//translate to center the quad at 0,0
	matrix=realityMatrixTranslation(-(600+400),-(300+300),0);

	//rotate through z axis
	tmp=realityMatrixRotationZ(ang);
	matrix=realityMatrixMul(&matrix,&tmp);

	//move to original position
	tmp=realityMatrixTranslation(600+400,300+300,0);
	matrix=realityMatrixMul(&matrix,&tmp);


	realityViewportTranslate(context, 0.0, 0.0, 0.0, 0.0);
	realityViewportScale(context, 1.0, 1.0, 1.0, 0.0); 

	realityZControl(context, 0, 1, 1); // disable viewport culling

	// Enable alpha blending.
	realityBlendFunc(context,
		NV30_3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA |
		NV30_3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
		NV30_3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA |
		NV30_3D_BLEND_FUNC_DST_ALPHA_ZERO);
	realityBlendEquation(context, NV40_3D_BLEND_EQUATION_RGB_FUNC_ADD |
		NV40_3D_BLEND_EQUATION_ALPHA_FUNC_ADD);
	realityBlendEnable(context, 1);

	realityViewport(context, res.width, res.height);

	setupRenderTarget(buffer);

	// set the clear color
	realitySetClearColor(context, 0x00000000); // Black, because it looks cool
	// and the depth clear value
	realitySetClearDepthValue(context, 0xffff);
	// Clear the buffers
	realityClearBuffers(context, REALITY_CLEAR_BUFFERS_COLOR_R |
				     REALITY_CLEAR_BUFFERS_COLOR_G |
				     REALITY_CLEAR_BUFFERS_COLOR_B |
				     NV30_3D_CLEAR_BUFFERS_COLOR_A |
				     NV30_3D_CLEAR_BUFFERS_STENCIL |
				     REALITY_CLEAR_BUFFERS_DEPTH);

	// Load shaders, because the rsx won't do anything without them.
	realityLoadVertexProgram(context, (realityVertexProgram*)vshader_bin);
	realityLoadFragmentProgram(context, &nv30_fp); 

	//Pass the matrix to the shader
	realitySetVertexProgramConstant4fBlock(context,matrixParam,4,(float*)(matrix.data));

	// Load texture
	load_tex(0, tx_offset, dice.width, dice.height, dice.width*4,  NV40_3D_TEX_FORMAT_FORMAT_A8R8G8B8, 1);
	
	// Generate quad
	realityVertexBegin(context, REALITY_QUADS);
	{
		realityTexCoord2f(context, 0.0, 0.0);
		realityVertex4f(context, 600.0, 300.0, 0.0, 1.0); 

		realityTexCoord2f(context, 1.0, 0.0);
		realityVertex4f(context, 1400.0, 300.0, 0.0, 1.0); 

		realityTexCoord2f(context, 1.0, 1.0);
		realityVertex4f(context, 1400.0, 900.0, 0.0, 1.0); 

		realityTexCoord2f(context, 0.0, 1.0);
		realityVertex4f(context, 600.0, 900.0, 0.0, 1.0); 
	}
	realityVertexEnd(context);
}

s32 main(s32 argc, const char* argv[])
{
	PadInfo padinfo;
	PadData paddata;
	int i;
	
	init_screen();
	ioPadInit(7);

	// Load texture
	dice = loadPng(dice_bin);
	assert(realityAddressToOffset(dice.data, &tx_offset) == 0);

	//load_acid_texture((uint8_t *)tx_mem, 0);

	// install fragment shader in rsx memory
	u32 *frag_mem = rsxMemAlign(256, 256);
	printf("frag_mem = 0x%08lx\n", (u64) frag_mem);
	realityInstallFragmentProgram(context, &nv30_fp, frag_mem);

	long frame = 0; // To keep track of how many frames we have rendered.
	
	// Ok, everything is setup. Now for the main loop.
	while(1){
		// Check the pads.
		ioPadGetInfo(&padinfo);
		for(i=0; i<MAX_PADS; i++){
			if(padinfo.status[i]){
				ioPadGetData(i, &paddata);
				
				if(paddata.BTN_CROSS){
					return 0;
				}
			}
			
		}

		waitFlip(); // Wait for the last flip to finish, so we can draw to the old buffer
		drawFrame(currentBuffer, frame++); // Draw into the unused buffer
		flip(currentBuffer); // Flip buffer onto screen
		currentBuffer = !currentBuffer;

	}
	
	return 0;
}
