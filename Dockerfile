FROM debian:latest

RUN apt-get update && apt-get install -y \
	lua5.4 lua5.3 lua5.2 lua5.1 \
	build-essential \
	liblua5.4-dev liblua5.3-dev liblua5.2-dev liblua5.1-dev \
	luarocks \
	libpulse-dev \
	&& apt-get clean \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install dependencies for all Lua versions
RUN luarocks --lua-version 5.4 install lua_pa && luarocks --lua-version 5.4 install luasocket
RUN luarocks --lua-version 5.3 install lua_pa && luarocks --lua-version 5.3 install luasocket
RUN luarocks --lua-version 5.2 install lua_pa && luarocks --lua-version 5.2 install luasocket
RUN luarocks --lua-version 5.1 install lua_pa && luarocks --lua-version 5.1 install luasocket

# Set environment variables for LUA_PATH and LUA_CPATH
ENV LUA_PATH="/usr/local/share/lua/?.lua;/usr/local/share/lua/?/init.lua;;"
ENV LUA_CPATH="/usr/local/lib/lua/?.so;/usr/local/lib/lua/socket/?.so;;"

# Copy test script
COPY test.lua .

# Create a script to run the test for all Lua versions
RUN echo '#!/bin/bash\n' \
	'for lua_version in lua5.4 lua5.3 lua5.2 lua5.1; do\n' \
	'  echo "Running test with $lua_version:"\n' \
	'  $lua_version test.lua || exit 1\n' \
	'done' > runtests.sh && chmod +x runtests.sh

# Run the test script for all Lua versions
CMD ["./runtests.sh"]
