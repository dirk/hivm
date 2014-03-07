def include_env v
  ENV[v].to_s
end

$ldflags = "-lpthread -L. #{include_env 'LDFLAGS'}".strip
$cflags  = "-g -Wall -std=c99 -I. #{include_env 'CFLAGS'}".strip

desc "Build"
task "build" => ["libhivem.a"]
task "default" => ["build"]

namespace "build" do
  desc "Build include directory (header files)"
  task "include" do
    headers = {
      "vm" => "hvm",
      "object" => "hvm_object"
    }
    headers.each do |src, dst|
      sh "cp src/#{src}.h include/#{dst}.h"
    end
  end
end

# desc "Compile"
file 'libhivem.a' => [
  # Source
  'src/vm.o', 'src/object.o'
] do |t|
  # sh "cc -o #{t.name} #{t.prerequisites.join ' '} #{LDFLAGS} #{CFLAGS}"
  sh "ar rcs #{t.name} #{t.prerequisites.join ' '}"
end

rule '.o' => ['.c'] do |t|
  sh "cc #{t.source} -c #{$cflags} -o #{t.name}"
end

desc "Clean up objects"
task "clean" do
  sh "rm src/*.o"
  # sh "rm test/*.o"
  sh "rm libhivem.a"
end

namespace "clean" do
  desc "Clean up everything (objects, docs)"
  task "all" => ["clean", "doc:clean"] do
    sh "rm include/*.h"
    puts 'Done!'
  end
end

desc "Build documentation"
task "doc" do
  sh "doxygen Doxyfile"
end

namespace "doc" do
  desc "Clean up documentation"
  task "clean" do
    sh "rm -rf doc/*"
  end
end
