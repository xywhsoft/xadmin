--
-- SQLiteStudio v3.4.17 生成的文件，周三 1月 7 21:29:41 2026
--
-- 所用的文本编码：UTF-8
--
PRAGMA foreign_keys = off;
BEGIN TRANSACTION;

-- 表：auth
CREATE TABLE auth (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    groupID    INTEGER,
    name       TEXT,
    desc       TEXT,
    sort       INTEGER,
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     1,
                     '未分组',
                     '未经分组的 URI 会被归类到这个分类下',
                     0,
                     63930019316,
                     63935032474,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     2,
                     1,
                     '服务管理',
                     '[服务管理] 页面和接口',
                     1000,
                     63933802678,
                     63933903248,
                     1
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     3,
                     5,
                     '基础页面',
                     '主页、登录页面等非功能归类页面',
                     100000,
                     63933903210,
                     63935032468,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     4,
                     1,
                     '服务列表',
                     '[任务管理] 页面和接口',
                     2000,
                     63933903275,
                     63933903275,
                     1
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     5,
                     3,
                     '系统日志',
                     '[系统日志] 页面和接口',
                     500000,
                     63933903308,
                     63935032434,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     6,
                     4,
                     '后台用户管理',
                     '[权限管理] - [用户管理] 页面和接口',
                     700000,
                     63933903342,
                     63935033427,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     7,
                     4,
                     '后台角色管理',
                     '[权限管理] - [角色管理] 页面和接口',
                     701000,
                     63933903366,
                     63935033431,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     8,
                     4,
                     '后台权限分类',
                     '[权限管理] - [权限分类] 页面和接口',
                     702000,
                     63933903387,
                     63935033434,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     9,
                     4,
                     '后台权限分组',
                     '[权限管理] - [权限分组] 页面和接口',
                     703000,
                     63933903428,
                     63935033437,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     10,
                     7,
                     '接口管理',
                     '[设置] - [接口管理] 页面和接口',
                     802000,
                     63933903456,
                     63935032279,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     11,
                     7,
                     '设置管理',
                     '所有 [自定义设置] 页面和接口',
                     800000,
                     63934948478,
                     63935032338,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     12,
                     7,
                     '菜单管理',
                     '[设置] - [菜单管理] 页面和接口',
                     801000,
                     63934948510,
                     63935032255,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     13,
                     8,
                     '前台用户管理',
                     '[前台用户管理] - [用户管理] 页面和接口',
                     600000,
                     63934949282,
                     63935033458,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     14,
                     8,
                     '前台用户组管理',
                     '[前台用户管理] - [用户组管理] 页面和接口',
                     601000,
                     63934949319,
                     63935033454,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     15,
                     9,
                     '调试接口',
                     '用于调试的接口',
                     900000,
                     63934949645,
                     63935032233,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     16,
                     8,
                     '前台权限分类',
                     '[前台用户管理] - [权限分类] 页面和接口',
                     602000,
                     63934949844,
                     63935033451,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     17,
                     8,
                     '前台权限分组',
                     '[前台用户管理] - [权限分组] 页面和接口',
                     603000,
                     63934949844,
                     63935033445,
                     0
                 );

INSERT INTO auth (
                     id,
                     groupID,
                     name,
                     desc,
                     sort,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     18,
                     8,
                     'URI权限管理',
                     '[前台用户管理] - [URI权限] 页面和接口',
                     304000,
                     63934949844,
                     63934949844,
                     1
                 );


-- 表：authGroup
CREATE TABLE authGroup (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT,
    desc       TEXT,
    sort       INTEGER,
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          1,
                          '未分类',
                          '未分类的权限',
                          0,
                          63930028060,
                          63935032504,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          2,
                          '服务管理',
                          '[服务管理] 菜单',
                          1000,
                          63933807197,
                          63933903128,
                          1
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          3,
                          '系统日志',
                          '[系统日志] 菜单',
                          500000,
                          63933903025,
                          63935032210,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          4,
                          '后台权限管理',
                          '[后台权限管理] 菜单',
                          700000,
                          63933903052,
                          63935032198,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          5,
                          '基础权限',
                          '主页、登录等页面',
                          100000,
                          63933903097,
                          63935032501,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          6,
                          '服务列表',
                          '[服务列表] 菜单',
                          2000,
                          63933903142,
                          63933903142,
                          1
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          7,
                          '设置',
                          '[设置] 菜单',
                          800000,
                          63934948202,
                          63935032191,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          8,
                          '前台用户管理',
                          '[前台用户管理] 菜单',
                          600000,
                          63934948299,
                          63935032204,
                          0
                      );

INSERT INTO authGroup (
                          id,
                          name,
                          desc,
                          sort,
                          createTime,
                          updateTime,
                          isDelete
                      )
                      VALUES (
                          9,
                          '调试接口',
                          '用于调试的接口',
                          900000,
                          63934949629,
                          63935032126,
                          0
                      );


-- 表：logs
CREATE TABLE logs (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user       TEXT,
    ip         TEXT,
    uri        TEXT,
    method     TEXT,
    param      TEXT,
    body       TEXT,
    createTime INTEGER
);


-- 表：member
CREATE TABLE member (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    username   TEXT    UNIQUE,
    salt       TEXT,
    pwd        TEXT,
    groupId    INTEGER DEFAULT (1),
    authLevel  INTEGER DEFAULT (0),
    balance    INTEGER DEFAULT (0),
    nickname   TEXT,
    email      TEXT,
    phone      TEXT,
    avatar     TEXT,
    status     INTEGER DEFAULT (1),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO member (
                       id,
                       username,
                       salt,
                       pwd,
                       groupId,
                       authLevel,
                       balance,
                       nickname,
                       email,
                       phone,
                       avatar,
                       status,
                       createTime,
                       updateTime,
                       isDelete
                   )
                   VALUES (
                       1,
                       'demo',
                       'dM30uWu00001a1EiofyV4b9-_aHNO0aS',
                       'C9D3D3A8E5B8F9C7D6A5B4C3D2E1F0A9B8C7D6E5F4A3B2C1D0E9F8A7B6C5D4E3',
                       1,
                       0,
                       0,
                       '演示用户',
                       'demo@example.com',
                       '',
                       '',
                       1,
                       63935000000,
                       63935000000,
                       0
                   );


-- 表：memberAuth
CREATE TABLE memberAuth (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    groupID    INTEGER DEFAULT (1),
    name       TEXT,
    desc       TEXT,
    sort       INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO memberAuth (
                           id,
                           groupID,
                           name,
                           desc,
                           sort,
                           createTime,
                           updateTime,
                           isDelete
                       )
                       VALUES (
                           1,
                           2,
                           '基础接口',
                           '登录、注册、个人中心等基础接口',
                           0,
                           63935000000,
                           63935000000,
                           0
                       );

INSERT INTO memberAuth (
                           id,
                           groupID,
                           name,
                           desc,
                           sort,
                           createTime,
                           updateTime,
                           isDelete
                       )
                       VALUES (
                           2,
                           2,
                           'VIP功能',
                           'VIP专属功能接口',
                           100,
                           63935000000,
                           63935000000,
                           0
                       );


-- 表：memberAuthGroup
CREATE TABLE memberAuthGroup (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT,
    desc       TEXT,
    sort       INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO memberAuthGroup (
                                id,
                                name,
                                desc,
                                sort,
                                createTime,
                                updateTime,
                                isDelete
                            )
                            VALUES (
                                1,
                                '未分类',
                                '未分类的权限',
-                               1,
                                63935000000,
                                63935000000,
                                0
                            );

INSERT INTO memberAuthGroup (
                                id,
                                name,
                                desc,
                                sort,
                                createTime,
                                updateTime,
                                isDelete
                            )
                            VALUES (
                                2,
                                '基础功能',
                                '基础访问权限',
                                0,
                                63935000000,
                                63935000000,
                                0
                            );


-- 表：memberBalanceLog
CREATE TABLE memberBalanceLog (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    memberId   INTEGER,
    type       INTEGER DEFAULT (0),
    amount     INTEGER DEFAULT (0),
    balance    INTEGER DEFAULT (0),
    remark     TEXT,
    operator   TEXT,
    createTime INTEGER
);


-- 表：memberGroup
CREATE TABLE memberGroup (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT,
    desc       TEXT,
    authList   TEXT    DEFAULT '[]',
    authLevel  INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO memberGroup (
                            id,
                            name,
                            desc,
                            authList,
                            authLevel,
                            createTime,
                            updateTime,
                            isDelete
                        )
                        VALUES (
                            1,
                            '普通用户',
                            '默认用户组，拥有基础访问权限',
                            '[1]',
                            0,
                            63935000000,
                            63935000000,
                            0
                        );

INSERT INTO memberGroup (
                            id,
                            name,
                            desc,
                            authList,
                            authLevel,
                            createTime,
                            updateTime,
                            isDelete
                        )
                        VALUES (
                            2,
                            'VIP用户',
                            'VIP用户组，拥有更多访问权限',
                            '[1,2]',
                            200,
                            63935000000,
                            63935032073,
                            0
                        );


-- 表：memberUris
CREATE TABLE memberUris (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    authID     INTEGER DEFAULT (1),
    uri        TEXT    UNIQUE,
    desc       TEXT,
    sort       INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER
);


-- 表：menu
CREATE TABLE menu (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    parent     INTEGER DEFAULT (0),
    title      TEXT,
    icon       TEXT,
    type       INTEGER DEFAULT (1),
    openType   TEXT    DEFAULT '_component',
    href       TEXT,
    sort       INTEGER DEFAULT (0),
    visible    INTEGER DEFAULT (1),
    remark     TEXT,
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER DEFAULT (0) 
);

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     0,
                     '系统日志',
                     'layui-icon layui-icon-log',
                     1,
                     '_component',
                     '/admin/view/logs',
                     100000,
                     1,
                     '查看系统日志',
                     63933640350,
                     63933640350,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     2,
                     0,
                     '后台权限管理',
                     'layui-icon layui-icon-auz',
                     0,
                     '',
                     '',
                     400000,
                     1,
                     '权限管理目录',
                     63933640350,
                     63934948566,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     3,
                     2,
                     '用户管理',
                     'layui-icon layui-icon-username',
                     1,
                     '_component',
                     '/admin/view/auth/user',
                     400100,
                     1,
                     '管理后台用户',
                     63933640350,
                     63934948571,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     4,
                     2,
                     '角色管理',
                     'layui-icon layui-icon-user',
                     1,
                     '_component',
                     '/admin/view/auth/role',
                     400200,
                     1,
                     '管理用户角色',
                     63933640350,
                     63934948585,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     5,
                     2,
                     '权限分类',
                     'layui-icon layui-icon-tabs',
                     1,
                     '_component',
                     '/admin/view/auth/group',
                     400300,
                     1,
                     '管理权限分类',
                     63933640350,
                     63934948581,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     6,
                     2,
                     '权限分组',
                     'layui-icon layui-icon-template',
                     1,
                     '_component',
                     '/admin/view/auth/auth',
                     400400,
                     1,
                     '管理权限分组',
                     63933640350,
                     63934948578,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     7,
                     10,
                     '接口管理',
                     'layui-icon layui-icon-website',
                     1,
                     '_component',
                     '/admin/view/auth/uris',
                     400500,
                     1,
                     '管理API接口',
                     63933640350,
                     63935026581,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     8,
                     10,
                     '菜单管理',
                     'layui-icon layui-icon-spread-left',
                     1,
                     '_component',
                     '/admin/view/option/menu',
                     203000,
                     1,
                     '管理系统菜单',
                     63933640350,
                     63934948612,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     9,
                     10,
                     '全局配置',
                     'layui-icon layui-icon-set',
                     1,
                     '_iframe',
                     '/admin/view/option?file=global.json',
                     201000,
                     1,
                     '系统全局配置管理',
                     63933640350,
                     63934948596,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     10,
                     0,
                     '设置',
                     'layui-icon layui-icon-set',
                     0,
                     '',
                     '',
                     500000,
                     1,
                     '设置目录',
                     63934847322,
                     63935026669,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     11,
                     10,
                     '自定义设置',
                     'layui-icon layui-icon-face-smile',
                     1,
                     '_iframe',
                     '/admin/view/option?file=my.json',
                     202000,
                     1,
                     '',
                     63934855728,
                     63934948603,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     12,
                     0,
                     '前台用户管理',
                     'layui-icon layui-icon-friends',
                     0,
                     '',
                     '',
                     300000,
                     1,
                     '前台用户管理目录',
                     63934949844,
                     63934949844,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     13,
                     12,
                     '用户管理',
                     'layui-icon layui-icon-username',
                     1,
                     '_component',
                     '/admin/view/member/user',
                     300100,
                     1,
                     '管理前台用户',
                     63934949844,
                     63934949844,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     14,
                     12,
                     '用户组管理',
                     'layui-icon layui-icon-group',
                     1,
                     '_component',
                     '/admin/view/member/group',
                     300200,
                     1,
                     '管理前台用户组',
                     63934949844,
                     63934949844,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     15,
                     12,
                     '权限分类',
                     'layui-icon layui-icon-tabs',
                     1,
                     '_component',
                     '/admin/view/member/authgroup',
                     300300,
                     1,
                     '管理前台权限分类',
                     63934949844,
                     63934949844,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     16,
                     12,
                     '权限分组',
                     'layui-icon layui-icon-vercode',
                     1,
                     '_component',
                     '/admin/view/member/auth',
                     300400,
                     1,
                     '管理前台权限分组',
                     63934949844,
                     63934949844,
                     0
                 );

INSERT INTO menu (
                     id,
                     parent,
                     title,
                     icon,
                     type,
                     openType,
                     href,
                     sort,
                     visible,
                     remark,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     17,
                     0,
                     '内容模型',
                     'layui-icon layui-icon-component',
                     1,
                     '_component',
                     '/admin/view/model',
                     200000,
                     1,
                     '内容模型管理，可定义自定义内容结构',
                     63935100000,
                     63935100000,
                     0
                 );


-- 表：role
CREATE TABLE role (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT,
    desc       TEXT,
    authList   TEXT    DEFAULT "[]",
    authLevel  INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER
);

INSERT INTO role (
                     id,
                     name,
                     desc,
                     authList,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     '超级管理员',
                     '超级管理员账户，拥有后台的全部权限',
                     '[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18]',
                     999,
                     63930031353,
                     63934938596,
                     0
                 );

INSERT INTO role (
                     id,
                     name,
                     desc,
                     authList,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     2,
                     '管理员',
                     '拥有除了权限管理外的所有权限',
                     '[3,13,14,16,17,11]',
                     200,
                     63930031443,
                     63935040521,
                     0
                 );


-- 表：uris
CREATE TABLE uris (
    id         INTEGER PRIMARY KEY AUTOINCREMENT
                       UNIQUE,
    authID     INTEGER REFERENCES auth (id),
    uri        TEXT    UNIQUE,
    desc       TEXT,
    isBackend  INTEGER DEFAULT 1,
    needAuth   INTEGER DEFAULT 1,
    needLog    INTEGER DEFAULT 0,
    keepActive INTEGER DEFAULT 0,
    sort       INTEGER,
    createTime INTEGER,
    updateTime INTEGER
);

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     1,
                     3,
                     '/admin/login',
                     '后台 [用户登录] 主接口',
                     1,
                     0,
                     1,
                     0,
                     1100000,
                     63935023250,
                     63935033999
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     2,
                     3,
                     '/admin',
                     '管理后台',
                     1,
                     1,
                     0,
                     1,
                     1100010,
                     63933638061,
                     63933904949
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     3,
                     3,
                     '/admin/view/home',
                     '管理后台主页',
                     1,
                     1,
                     1,
                     1,
                     1100020,
                     63933638061,
                     63933904962
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     4,
                     3,
                     '/admin/menu',
                     '管理后台左侧菜单数据接口',
                     1,
                     1,
                     0,
                     0,
                     1100030,
                     63933645834,
                     63933904968
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     5,
                     3,
                     '/admin/logout',
                     '后台 [注销登录] 主接口',
                     1,
                     1,
                     0,
                     0,
                     1100040,
                     63933638061,
                     63933904983
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     6,
                     5,
                     '/admin/view/logs',
                     '[系统日志] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1500000,
                     63933640875,
                     63933904827
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     7,
                     5,
                     '/admin/logs',
                     '[系统日志] 主接口',
                     1,
                     1,
                     0,
                     1,
                     1500010,
                     63933638061,
                     63933904869
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     8,
                     5,
                     '/admin/logs/clear',
                     '[系统日志] 清除 7 天前日志接口',
                     1,
                     1,
                     1,
                     1,
                     1500020,
                     63933638061,
                     63933904915
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     9,
                     13,
                     '/admin/view/member/user',
                     '前台 [用户管理] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1600000,
                     63935017487,
                     63935033705
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     10,
                     13,
                     '/admin/member/user',
                     '前台 [用户管理] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1600010,
                     63935017488,
                     63935033698
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     11,
                     13,
                     '/admin/view/member/user/add',
                     '前台 [用户管理] - [添加用户] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1600020,
                     63935017487,
                     63935033716
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     12,
                     13,
                     '/admin/view/member/user/edit',
                     '前台 [用户管理] - [编辑用户] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1600030,
                     63935017487,
                     63935033724
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     13,
                     13,
                     '/admin/member/user/repwd',
                     '前台 [用户管理] 无条件重置密码接口（危险权限）',
                     1,
                     1,
                     1,
                     1,
                     1600040,
                     63935017488,
                     63935033736
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     14,
                     13,
                     '/admin/member/user/balance',
                     '前台 [用户管理] 余额调整接口（危险权限）',
                     1,
                     1,
                     1,
                     1,
                     1600050,
                     63935017488,
                     63935033749
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     15,
                     14,
                     '/admin/view/member/group',
                     '前台 [用户组管理] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1601000,
                     63935017487,
                     63935033606
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     16,
                     14,
                     '/admin/member/group',
                     '前台 [用户组管理] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1601010,
                     63935017488,
                     63935033642
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     17,
                     14,
                     '/admin/view/member/group/add',
                     '前台 [用户组管理] - [添加角色] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1601020,
                     63935017488,
                     63935033659
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     18,
                     14,
                     '/admin/view/member/group/edit',
                     '前台 [用户组管理] - [编辑角色] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1601030,
                     63935017488,
                     63935033669
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     19,
                     16,
                     '/admin/view/member/authgroup',
                     '前台 [权限分类] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1602000,
                     63935017488,
                     63935033515
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     20,
                     16,
                     '/admin/member/authgroup',
                     '前台 [权限分类] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1602010,
                     63935017488,
                     63935033530
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     21,
                     16,
                     '/admin/view/member/authgroup/add',
                     '前台 [权限分类] - [添加分类] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1602020,
                     63935017487,
                     63935033546
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     22,
                     16,
                     '/admin/view/member/authgroup/edit',
                     '前台 [权限分类] - [编辑分类] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1602030,
                     63935017488,
                     63935033568
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     23,
                     17,
                     '/admin/view/member/auth',
                     '前台 [权限分组] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1603000,
                     63935017487,
                     63935033369
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     24,
                     17,
                     '/admin/member/auth',
                     '前台 [权限分组] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1603010,
                     63935017488,
                     63935033377
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     25,
                     17,
                     '/admin/view/member/auth/add',
                     '前台 [权限分组] - [添加分组] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1603020,
                     63935017487,
                     63935033382
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     26,
                     17,
                     '/admin/view/member/auth/edit',
                     '前台 [权限分组] - [编辑分组] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1603030,
                     63935017487,
                     63935033386
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     27,
                     6,
                     '/admin/view/auth/user',
                     '后台 [用户管理] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1700000,
                     63933638061,
                     63933904184
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     28,
                     6,
                     '/admin/auth/user',
                     '后台 [用户管理] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1700010,
                     63933638061,
                     63933904407
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     29,
                     6,
                     '/admin/view/auth/user/add',
                     '后台 [用户管理] - [添加用户] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1700020,
                     63933814573,
                     63933904510
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     30,
                     6,
                     '/admin/view/auth/user/edit',
                     '后台 [用户管理] - [编辑用户] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1700030,
                     63933814573,
                     63933904534
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     31,
                     6,
                     '/admin/auth/user/repwd',
                     '后台 [用户管理] 无条件重置密码接口（危险权限）',
                     1,
                     1,
                     1,
                     1,
                     1700040,
                     63933638061,
                     63933904541
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     32,
                     7,
                     '/admin/view/auth/role',
                     '后台 [角色管理] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1701000,
                     63933638061,
                     63933904214
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     33,
                     7,
                     '/admin/auth/role',
                     '后台 [角色管理] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1701010,
                     63933638061,
                     63933904366
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     34,
                     7,
                     '/admin/view/auth/role/add',
                     '后台 [角色管理] - [添加角色] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1701020,
                     63933809706,
                     63933904571
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     35,
                     7,
                     '/admin/view/auth/role/edit',
                     '后台 [角色管理] - [编辑角色] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1701030,
                     63933809706,
                     63933904591
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     36,
                     8,
                     '/admin/view/auth/group',
                     '后台 [权限分类] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1702000,
                     63933638061,
                     63933904174
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     37,
                     8,
                     '/admin/auth/group',
                     '后台 [权限分类] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1702010,
                     63933638061,
                     63933904326
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     38,
                     8,
                     '/admin/view/auth/group/add',
                     '后台 [权限分类] - [添加分类] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1702020,
                     63933805653,
                     63933904697
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     39,
                     8,
                     '/admin/view/auth/group/edit',
                     '后台 [权限分类] - [编辑分类] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1702030,
                     63933805653,
                     63933904710
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     40,
                     9,
                     '/admin/view/auth/auth',
                     '后台 [权限分组] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1703000,
                     63933638060,
                     63933904188
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     41,
                     9,
                     '/admin/auth/auth',
                     '后台 [权限分组] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1703010,
                     63933638061,
                     63933904381
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     42,
                     9,
                     '/admin/view/auth/auth/add',
                     '后台 [权限分组] - [添加分组] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1703020,
                     63933720099,
                     63933904731
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     43,
                     9,
                     '/admin/view/auth/auth/edit',
                     '后台 [权限分组] - [编辑分组] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1703030,
                     63933720099,
                     63933904753
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     44,
                     11,
                     '/admin/view/option',
                     '[设置管理] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1800000,
                     63934828369,
                     63934949414
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     45,
                     11,
                     '/admin/option',
                     '[设置管理] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1800010,
                     63934828369,
                     63934949473
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     46,
                     12,
                     '/admin/view/option/menu',
                     '[菜单管理] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1801000,
                     63934829866,
                     63934949497
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     47,
                     12,
                     '/admin/option/menu',
                     '[菜单管理] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1801010,
                     63934829866,
                     63934949520
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     48,
                     12,
                     '/admin/view/option/menu/add/category',
                     '[菜单管理] - [添加分类] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1801020,
                     63934843214,
                     63934949555
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     49,
                     12,
                     '/admin/view/option/menu/add',
                     '[菜单管理] - [添加菜单] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1801030,
                     63934829866,
                     63934949575
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     50,
                     12,
                     '/admin/view/option/menu/edit',
                     '[菜单管理] - [编辑菜单] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1801040,
                     63934829866,
                     63934949602
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     51,
                     10,
                     '/admin/view/auth/uris',
                     '[接口管理] 主页面视图',
                     1,
                     1,
                     1,
                     1,
                     1802000,
                     63933638060,
                     63933904195
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     52,
                     10,
                     '/admin/auth/uris',
                     '[接口管理] 主接口',
                     1,
                     1,
                     1,
                     1,
                     1802010,
                     63933638061,
                     63933904305
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     53,
                     10,
                     '/admin/view/auth/uris/edit',
                     '[接口管理] - [编辑接口] 子页面视图',
                     1,
                     1,
                     1,
                     1,
                     1802020,
                     63933638061,
                     63934948331
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     54,
                     15,
                     '/admin/trace',
                     '获取全局缓存概览',
                     1,
                     1,
                     0,
                     1,
                     1900000,
                     63934856182,
                     63935032523
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     55,
                     15,
                     '/admin/trace/session',
                     '获取完整的 Session 缓存数据',
                     1,
                     1,
                     0,
                     1,
                     1900010,
                     63934856181,
                     63935032527
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     56,
                     15,
                     '/admin/trace/option',
                     '获取完整的 Option 配置缓存数据',
                     1,
                     1,
                     0,
                     1,
                     1900020,
                     63934856182,
                     63935032531
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     57,
                     15,
                     '/admin/trace/auth',
                     '获取权限相关缓存数据',
                     1,
                     1,
                     0,
                     1,
                     1900030,
                     63934856181,
                     63935032534
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     58,
                     15,
                     '/admin/trace/route',
                     '获取路由表数据',
                     1,
                     1,
                     0,
                     1,
                     1900040,
                     63934856181,
                     63935032538
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     59,
                     1,
                     '/api/v1/register',
                     '前台 [用户注册] 主接口',
                     0,
                     0,
                     0,
                     0,
                     100000,
                     63935023250,
                     63935034045
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     60,
                     1,
                     '/api/v1/login',
                     '前台 [用户登录] 主接口',
                     0,
                     0,
                     0,
                     0,
                     100010,
                     63935023250,
                     63935034051
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     61,
                     1,
                     '/api/v1/logout',
                     '前台 [注销登录] 主接口',
                     0,
                     1,
                     0,
                     0,
                     100020,
                     63935017487,
                     63935034062
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     62,
                     1,
                     '/api/v1/profile',
                     '前台 [获取当前用户信息] 接口',
                     0,
                     1,
                     0,
                     1,
                     100030,
                     63935017488,
                     63935034077
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     63,
                     1,
                     '/api/v1/profile/password',
                     '前台 [修改当前用户密码] 接口',
                     0,
                     1,
                     0,
                     1,
                     100040,
                     63935017487,
                     63935034083
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     64,
                     1,
                     '/api/v1/balance',
                     '前台 [获取当前用户余额信息] 接口',
                     0,
                     1,
                     0,
                     1,
                     100050,
                     63935017488,
                     63935034088
                 );

INSERT INTO uris (
                     id,
                     authID,
                     uri,
                     desc,
                     isBackend,
                     needAuth,
                     needLog,
                     keepActive,
                     sort,
                     createTime,
                     updateTime
                 )
                 VALUES (
                     65,
                     1,
                     '/api/v1/balance/log',
                     '前台 [获取当前用户余额日志] 接口',
                     0,
                     1,
                     0,
                     1,
                     100060,
                     63935017487,
                     63935034093
                 );


-- 表：user
CREATE TABLE user (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user       TEXT,
    salt       TEXT,
    pwd        TEXT,
    role       INTEGER,
    authLevel  INTEGER DEFAULT (0),
    createTime INTEGER,
    updateTime INTEGER,
    isDelete   INTEGER
);

INSERT INTO user (
                     id,
                     user,
                     salt,
                     pwd,
                     role,
                     authLevel,
                     createTime,
                     updateTime,
                     isDelete
                 )
                 VALUES (
                     1,
                     'admin',
                     'yT30uWu00001a1EiofyV4b9-_aHNO0aS',
                     '4FD681049123E6D9E48D534D7AA7DACEDE38C8860B14069647FF1E4446573521',
                     1,
                     0,
                     63933640350,
                     63933829361,
                     0
                 );


COMMIT TRANSACTION;
PRAGMA foreign_keys = on;
