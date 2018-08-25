{
    "targets": [
        {
            "target_name": "sickle_core",
            "sources": [
                "sickle_core.cpp",
            ],
            "include_dirs": [
                "<!(node -e \"require('nan')\")"
            ]
        }
    ]
}
