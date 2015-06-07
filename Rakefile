require "rake/clean"

HTMLSPLIT_VERSION = File.read("CMakeLists.txt").match(/^set\(HTMLSPLIT_VERSION "(.*?)"\)$/)[1]
PROJECT_DIR = File.expand_path(File.dirname(__FILE__))

CLOBBER.include("build")

file "../htmlsplit-#{HTMLSPLIT_VERSION}.tar.gz" => [:clobber] + FileList["**/*"] do |t|
  rel_projdir = File.basename(PROJECT_DIR)
  rel_verdir  = "htmlsplit-#{HTMLSPLIT_VERSION}"

  cd ".." do
    ln_s rel_projdir, rel_verdir
    sh "tar --exclude '.git*' -czhf #{File.basename(t.name)} \"#{rel_verdir}\""
    rm rel_verdir
  end

  puts "Build release tarball in ../#{File.basename(t.name)}."
end

desc "Build a release tarball."
task :tarball => "../htmlsplit-#{HTMLSPLIT_VERSION}.tar.gz"
