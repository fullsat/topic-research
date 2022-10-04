#[derive(Debug)]
struct Q1 {
    q1_1: i32,
    q1_2: Q1m,
    q1_3: i32,
    q1_4: Q1m,
}

#[derive(Debug)]
struct Q1m {
    q1m_1: i32,
}

fn main() {
    let q1 = Q1{
        q1_1: 1,
        q1_2: Q1m{
            q1m_1: 32
        },
        q1_3: 32,
        q1_4: Q1m{
            q1m_1: 32
        },
    };

    // Q1-1
    let reasign_q1_1 = q1.q1_1;
    println!("{}", reasign_q1_1);
    println!("{}", q1.q1_1);
    println!("{:#?}", q1);

    // Q1-2
    println!("-----");
    let reasign_q1_2 = q1.q1_2;
    println!("{:#?}", reasign_q1_2);
    println!("{:#?}", q1.q1_2);
    println!("{:#?}", q1);
}
