
def include_env v
  ENV[v].to_s
end

def cflags_for file
  basename = File.basename file
  flags = CFLAGS
  if %w{object.o generator.o exception.o debug.o bootstrap.o}.include? basename
    flags += " #{`pkg-config --cflags glib-2.0`.strip}"
  end
  if basename == 'object.o' && IS_LINUX
    flags += ' -I/usr/local/include'
  end
  # TODO: Refactor to make this unnecessary
  if basename == "generator.o" || basename == "bootstrap.o" || basename == "debug.o"
    flags += ' -Wno-unused-parameter'
  end
  if basename == "vm-db.o" || basename == "debug.o"
    flags += " -DHVM_VM_DEBUG #{`pkg-config --cflags #{LUA}`.strip}"
  end
  if basename == "jit-compiler.o"
    flags += " "+`#{LLVM_CONFIG} --cflags`.strip
  end
  # Optimization passes on some objects
  if basename =~ /^vm/ || basename == "gc1.o"
    flags += ' -O2'
  end
  return flags
end

# Search in a few default places for the jemalloc static library
def find_jemalloc
  paths = [
    '/usr/local/lib/libjemalloc.a',
    '/usr/local/libjemalloc.a'
  ]
  paths.each do |p|
    return p if File.exists? p
  end
  raise 'libjemalloc.a not found'
end
