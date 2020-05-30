#!/bin/bash
cd "${0%/*}"
echo "Regenerating wiiu_shaders.c ..."
latte-assembler compile colorShader.gsh --vsh colorShader.vsh --psh colorShader.psh
latte-assembler compile textureShader.gsh --vsh textureShader.vsh --psh textureShader.psh
xxd -i colorShader.gsh > colorShader.gsh.h
xxd -i textureShader.gsh > textureShader.gsh.h
echo "Done!"
