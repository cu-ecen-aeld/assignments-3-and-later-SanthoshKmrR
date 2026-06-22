
#!/bin/bash
 
if [ $# -ne 2 ]

then

	echo "Error : Two arguments needed : Filepath and text string"

	exit 1

fi
 
writefile=$1

writestr=$2
 
pathofdir=$(dirname "$writefile")
 
if [ ! -d "$pathofdir" ]

then

	mkdir -p "$pathofdir"

fi
 
echo "$writestr" > "$writefile"
 
if [ $? -ne 0 ]
then
	echo "Error : Unable to create file"
	exit 1
fi

exit 0
