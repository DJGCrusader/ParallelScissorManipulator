
PROJECT := gantry

   .PHONY : clean 

${PROJECT}: 

clean: 
	rm ${PROJECT}

CML:
	cd ../..; make

% : %.cpp CML
	g++ -o $@ -ggdb3 -I../../inc -L../.. $< -l MotionLib -lpthread -lrt
