project(
	'wf-panel-focus',
	'c',
	'cpp',
	version: '0.10.0',
	license: 'MIT',
	meson_version: '>=0.51.0',
	default_options: [
		'cpp_std=c++17',
		'c_std=c11',
	],
)

wayfire = dependency('wayfire')

add_project_arguments(['-DWLR_USE_UNSTABLE'], language: ['cpp', 'c'])

shared_module('panel-focus', 'panel-focus.cpp',
    dependencies: wayfire,
    install: true, install_dir: join_paths(get_option('libdir'), 'wayfire'))

install_data('panel-focus.xml', install_dir: wayfire.get_variable(pkgconfig: 'metadatadir'))
