
PROJECT := iofile

   .PHONY : CML clean 

${PROJECT}: 

clean: 
	rm ${PROJECT}

CML:
	cd ../..; make

% : %.cpp CML
	g++ -g -o $@ -ggdb3 -I../../inc -L../.. $< -l MotionLib -lpthread -lrt

