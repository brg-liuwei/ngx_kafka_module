require 'rake'

namespace :nginx do
  desc "Starts NGINX"
  task :start do
    `build/nginx/sbin/nginx`
    sleep 1
  end

  desc "Stops NGINX"
  task :stop do
    `build/nginx/sbin/nginx -s stop`
  end

  desc "Recompiles NGINX"
  task :compile do
    sh "PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$PWD/build/librdkafka/lib/pkgconfig scripts/compile"
  end
end

desc "Bootstraps the local development environment"
task :bootstrap do
  sh "scripts/bootstrap"
end

namespace :bootstrap do
  desc "Cleans vendor code"
  task :clean do
    sh "scripts/bootstrap clean"
  end

end
