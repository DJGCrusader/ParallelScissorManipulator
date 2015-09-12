
PROJECT := sixaxis 

   .PHONY : CML clean 

${PROJECT}: 

clean: 
	rm ${PROJECT}

CML:
	cd ../..; make

% : %.cpp CML
	g++ -g -o $@ -ggdb3 -I../../inc -I../../inc/can -L../.. $< -l MotionLib -lpthread -lrt -ldl