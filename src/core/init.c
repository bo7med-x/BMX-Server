/*
	This file is part of BMX Server.

    BMX Server is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BMX Server is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BMX Server.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "shared.h"

void BMX_Init();

int __attribute__((visibility ("default"))) BMX_ModuleLoad() {
	BMX_Init();
	return BUILDNUMBER;
}

void __attribute__ ((constructor)) __attribute__((visibility ("default"))) BMX_OnLoad(void) {
	BMX_Init();
}
// Odd, isn't it? 1.5 destructor works (apparently it should work but the stdout is closed or something... cba to rewrite for now)
#if CODPATCH == 5
void __attribute__ ((destructor)) __attribute__((visibility ("default"))) BMX_OnUnload( void ) {
	COD_Destructor();
}
#endif