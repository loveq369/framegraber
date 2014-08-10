all:
	gcc frames.c -DVERBOSE -lavformat -lavcodec -lavutil -lswscale `freetype-config --cflags` `freetype-config --libs` -o frames

clean:
	rm ./frames

