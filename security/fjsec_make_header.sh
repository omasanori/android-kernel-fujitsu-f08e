#! /bin/sh
# /*----------------------------------------------------------------------------*/
# // COPYRIGHT(C) FUJITSU LIMITED 2012
# /*----------------------------------------------------------------------------*/

OUTPUT_FILE="./kernel/security/fjsec_modinfo.h"

chmod a+w ${OUTPUT_FILE}
echo "/* This file is auto generated. */" > ${OUTPUT_FILE}

for file in $(find $1 -type f -name "*.ko"); do
	MOD_NAME=$(basename $file ".ko" | tr '[a-z]' '[A-Z]' | sed "s/\-/_/g")
	MOD_CHECKSUM=$(sha256sum $file | cut -d ' ' -f 1 | sed "s/\([a-z0-9]\{2\}\)/0x\1, /g")
	echo "#define CHECKSUM_${MOD_NAME} { ${MOD_CHECKSUM}}" >> ${OUTPUT_FILE}
done

