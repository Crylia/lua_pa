package = "lua_pa"
version = "1.0.0-1"
source = {
   url = "*** please add URL for source tarball, zip or repository here ***"
}
description = {
   homepage = "*** please enter a project homepage ***",
   license = "*** please specify a license ***"
}
dependencies = {
   "lua >= 5.1, < 5.5"
}
build = {
   type = "builtin",
   modules = {
      lua_pa = {
         sources = "src/lua_pa.c"
      }
   },
   install = {
      bin = {
         "bin/lua_pa.so"
      }
   }
}
